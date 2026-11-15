#include "codegen.hpp"

#include <algorithm>
#include <stdexcept>

bool CodeGenerator::generate(const Program &program)
{
    reset();

    for (const std::unique_ptr<Declaration> &declaration : program.declarations)
    {
        const FunctionDecl *function =
            dynamic_cast<const FunctionDecl *>(declaration.get());

        if (function != nullptr)
        {
            generateCallable(function->name, function->parameters,
                             function->returnType, function->body.get());
            continue;
        }

        const RulesetDecl *ruleset =
            dynamic_cast<const RulesetDecl *>(declaration.get());

        if (ruleset != nullptr)
        {
            generateCallable(ruleset->name, ruleset->parameters,
                             ruleset->returnType, ruleset->body.get());
            continue;
        }

        addError("Unsupported declaration in code generation");
    }

    // every field has its address so the total size is known
    // exported so a JavaScript host can read and write field data
    if (!memory_.empty() || hostReadsMemory_)
    {
        module_.setMemory(std::max<std::uint32_t>(memory_.pages(), 1));
    }

    if (errors_.empty())
    {
        wat_ = module_.str();
    }

    return errors_.empty();
}

void CodeGenerator::reset() {
    module_ = WatModule();
    wat_.clear();
    errors_.clear();
    function_ = nullptr;
    memory_ = MemoryLayout();
    importedFunctions_.clear();
    hostReadsMemory_ = false;
}

// host functions are imported as `(import "env" "<name>" (func $__<name> ...))`
// the "__" prefix is reserved, so they cannot clash with user functions
void CodeGenerator::requireImport(const std::string &name, const std::vector<WatType> &params)
{
    if (importedFunctions_.insert(name).second)
    {
        module_.importFunction("env", name, "__" + name, params, {});
    }
}

const std::string &CodeGenerator::getWat() const
{
    return wat_;
}

const std::vector<std::string> &CodeGenerator::getErrors() const
{
    return errors_;
}

void CodeGenerator::addError(const std::string &message)
{
    errors_.push_back(message);
}

// types

bool CodeGenerator::toWatType(const Type &type, WatType &result, const std::string &context)
{
    switch (type.kind)
    {
    case TypeKind::Int:
    case TypeKind::Bool: // booleans are 0 or 1
        result = WatType::I32;
        return true;

    case TypeKind::Float:
        result = WatType::F32;
        return true;

    case TypeKind::Void:
        addError("Void is not a value type (" + context + ")");
        return false;

    case TypeKind::Field: // the i32 address of the field's first element
        result = WatType::I32;
        return true;

    case TypeKind::Error:
        addError("Expression has no resolved type; run semantic analysis first (" + context + ")");
        return false;
    }

    return false;
}

// scopes and local naming

void CodeGenerator::enterScope()
{
    scopes_.emplace_back();
}

void CodeGenerator::exitScope()
{
    scopes_.pop_back();
}

// parameters keep their own names, since nothing else exists yet in the function
std::string CodeGenerator::declareParameter(const std::string &name)
{
    usedLocalNames_.insert(name);
    scopes_.back()[name] = name;
    return name;
}

// declares a local and returns the unique WAT name chosen for it
std::string CodeGenerator::declareLocal(const std::string &name, const Type &type)
{
    WatType watType;
    if (!toWatType(type, watType, "variable '" + name + "'"))
    {
        return name;
    }

    // x then x_1, x_2, ... etc.
    std::string candidate = name;
    int suffix = 1;
    while (usedLocalNames_.count(candidate) != 0)
    {
        candidate = name + "_" + std::to_string(suffix++);
    }

    usedLocalNames_.insert(candidate);
    scopes_.back()[name] = candidate;
    function_->addLocal(candidate, watType);

    return candidate;
}

std::string CodeGenerator::lookupLocal(const std::string &name)
{
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope)
    {
        auto found = scope->find(name);
        if (found != scope->end())
        {
            return found->second;
        }
    }

    addError("Unresolved variable '" + name + "'; run semantic analysis first");
    return name;
}


// functions and rulesets compile the same way: a WAT function with the same name,
// exported so a JavaScript host can call it. field parameters and results are
// i32 addresses (see generateFieldLet / generateCall for how they are copied)
void CodeGenerator::generateCallable(const std::string &name,
                                     const std::vector<Parameter> &parameters,
                                     const Type &returnType,
                                     const BlockStatement *body)
{
    std::vector<WatParam> params;
    bool valid = true;

    for (const Parameter &parameter : parameters)
    {
        WatType type;
        if (toWatType(parameter.type, type, "parameter '" + parameter.name + "'"))
        {
            params.push_back({parameter.name, type});
        }
        else
        {
            valid = false;
        }
    }

    std::vector<WatType> results;
    if (returnType.kind != TypeKind::Void)
    {
        WatType type;
        if (toWatType(returnType, type, "return type of '" + name + "'"))
        {
            results.push_back(type);
        }
        else
        {
            valid = false;
        }
    }

    if (!valid)
    {
        return;
    }

    function_ = &module_.addFunction(name, params, results);

    // fresh naming state for each function
    scopes_.clear();
    usedLocalNames_.clear();
    labelCounter_ = 0;

    // parameters and top level locals share one scope like semantic analysis
    enterScope();

    for (const WatParam &param : params)
    {
        declareParameter(param.name);
    }

    copyAssignedFieldParameters(parameters, body);

    if (body != nullptr)
    {
        generateBlock(*body, false);

        // WAT checks that the stack holds the result at the end of the function
        if (returnType.kind != TypeKind::Void)
        {
            bool endsInReturn =
                !body->statements.empty() &&
                dynamic_cast<const ReturnStatement *>(body->statements.back().get()) != nullptr;

            if (!endsInReturn)
            {
                function_->unreachable();
            }
        }
    }

    exitScope();

    module_.exportFunction(name);
    function_ = nullptr;
}

// statements

void CodeGenerator::generateBlock(const BlockStatement &block, bool createScope)
{
    if (createScope)
    {
        enterScope();
    }

    for (const std::unique_ptr<Statement> &statement : block.statements)
    {
        generateStatement(*statement);
    }

    if (createScope)
    {
        exitScope();
    }
}

void CodeGenerator::generateStatement(const Statement &statement)
{
    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        generateLet(*let);
        return;
    }

    if (const ReturnStatement *returnStatement = dynamic_cast<const ReturnStatement *>(&statement))
    {
        if (returnStatement->value != nullptr)
        {
            generateExpression(*returnStatement->value);
        }
        function_->returnFromFunction();
        return;
    }

    if (const ExpressionStatement *expressionStatement = dynamic_cast<const ExpressionStatement *>(&statement))
    {
        if (expressionStatement->expression != nullptr)
        {
            generateExpression(*expressionStatement->expression);

            // the value is unused, so remove it
            TypeKind kind = expressionStatement->expression->resolvedType.kind;
            if (kind != TypeKind::Void && kind != TypeKind::Error)
            {
                function_->drop();
            }
        }
        return;
    }

    if (const BlockStatement *block = dynamic_cast<const BlockStatement *>(&statement))
    {
        generateBlock(*block);
        return;
    }

    if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        if (assignment->value->resolvedType.kind == TypeKind::Field)
        {
            generateFieldAssignment(*assignment);
            return;
        }

        generateExpression(*assignment->value);
        function_->localSet(lookupLocal(assignment->name));
        return;
    }

    if (const IfStatement *ifStatement = dynamic_cast<const IfStatement *>(&statement))
    {
        generateIf(*ifStatement);
        return;
    }

    if (const WhileStatement *whileStatement = dynamic_cast<const WhileStatement *>(&statement))
    {
        generateWhile(*whileStatement);
        return;
    }

    addError("Unsupported statement in code generation");
}

void CodeGenerator::generateLet(const LetStatement &statement)
{
    if (statement.resolvedType.kind == TypeKind::Field)
    {
        generateFieldLet(statement);
        return;
    }

    if (statement.initializer != nullptr)
    {
        generateExpression(*statement.initializer);
    }

    std::string watName = declareLocal(statement.name, statement.resolvedType);

    if (statement.initializer != nullptr)
    {
        function_->localSet(watName);
    }
    // without an initializer the local keeps WAT's default of zero
}

void CodeGenerator::generateIf(const IfStatement &statement)
{
    generateExpression(*statement.condition);

    function_->beginIf();
    generateBlock(*statement.thenBlock);

    if (statement.elseBranch != nullptr)
    {
        function_->beginElse();
        // either a block or another if
        generateStatement(*statement.elseBranch);
    }

    function_->end();
}

// while cond { body } becomes:
//   block $exit
//     loop $again
//       <cond>  i32.eqz  br_if $exit (leave when the condition is false)
//       <body>
//       br $again (next iteration)
//     end
//   end
void CodeGenerator::generateWhile(const WhileStatement &statement)
{
    int id = labelCounter_++;
    std::string exitLabel = "while_exit_" + std::to_string(id);
    std::string loopLabel = "while_loop_" + std::to_string(id);

    function_->beginBlock(exitLabel);
    function_->beginLoop(loopLabel);

    generateExpression(*statement.condition);
    function_->i32EqualToZero();
    function_->branchIf(exitLabel);

    generateBlock(*statement.body);

    function_->branch(loopLabel);
    function_->end();
    function_->end();
}

// expressions

void CodeGenerator::generateExpression(const Expression &expression)
{
    if (const LiteralExpr *literal = dynamic_cast<const LiteralExpr *>(&expression))
    {
        generateLiteral(*literal);
        return;
    }

    if (const FieldLiteralExpr *field = dynamic_cast<const FieldLiteralExpr *>(&expression))
    {
        generateFieldLiteral(*field);
        return;
    }

    if (const VariableExpr *variable = dynamic_cast<const VariableExpr *>(&expression))
    {
        function_->localGet(lookupLocal(variable->name));
        return;
    }

    if (const UnaryExpr *unary = dynamic_cast<const UnaryExpr *>(&expression))
    {
        generateUnary(*unary);
        return;
    }

    if (const BinaryExpr *binary = dynamic_cast<const BinaryExpr *>(&expression))
    {
        generateBinary(*binary);
        return;
    }

    if (const CallExpr *call = dynamic_cast<const CallExpr *>(&expression))
    {
        // a returned field is the address of memory inside the callee, which the
        // next call to the same function overwrites. e.g. in g(f(1), f(2)) both
        // results would share one block, so the result is copied into a temporary
        // block for this call site:
        //   i32.const <temporary>   <call>   <bytes>   memory.copy   i32.const <temporary>
        if (call->resolvedType.kind == TypeKind::Field)
        {
            std::uint32_t temporary = 0;
            if (!reserveField(call->resolvedType, temporary, "result of '" + call->name + "'"))
            {
                return;
            }

            function_->i32Constant(static_cast<int>(temporary));
            generateCall(*call);
            copyField(call->resolvedType);
            function_->i32Constant(static_cast<int>(temporary));
            return;
        }

        generateCall(*call);
        return;
    }

    addError("Unsupported expression in code generation");
}

void CodeGenerator::generateLiteral(const LiteralExpr &literal)
{
    try
    {
        switch (literal.type.kind)
        {
        case TypeKind::Int:
            function_->i32Constant(std::stoi(literal.value));
            return;

        case TypeKind::Float:
            function_->f32Constant(std::stof(literal.value));
            return;

        case TypeKind::Bool:
            function_->i32Constant(literal.value == "true" ? 1 : 0);
            return;

        default:
            break;
        }
    }
    catch (const std::out_of_range &)
    {
        addError("Literal out of range: " + literal.value);
        return;
    }

    addError("Unsupported literal type for '" + literal.value + "'");
}

// [a, b, c] reserves fixed block of memory at compile time
// then at runtime we store each element into it
//
// i32.const <block> <a> f32.store
// i32.const <block> <b> f32.store offset=4
// i32.const <block> <c> f32.store offset=8
// i32.const <block>
//
// every element is 4 bytes (i32, f32 or bool)
// bools are stored as SIMD masks
//  true = -1 (all bits 1), false = 0 (all 0)
void CodeGenerator::generateFieldLiteral(const FieldLiteralExpr &literal)
{
    const Type &type = literal.resolvedType;

    if (type.kind != TypeKind::Field || type.elementType == nullptr)
    {
        addError("Field literal has no resolved type; run semantic analysis first");
        return;
    }

    std::uint32_t address = 0;
    if (!reserveField(type, address, "field literal"))
    {
        return;
    }

    writeFieldLiteral(literal, address);
    function_->i32Constant(static_cast<int>(address));
}

void CodeGenerator::writeFieldLiteral(const FieldLiteralExpr &literal, std::uint32_t address)
{
    const Type &type = literal.resolvedType;
    TypeKind elementKind = type.elementType->kind;

    for (std::size_t i = 0; i < literal.elements.size(); i++)
    {
        unsigned offset = static_cast<unsigned>(i * FIELD_ELEMENT_BYTES);

        function_->i32Constant(static_cast<int>(address));

        switch (elementKind)
        {
        case TypeKind::Int:
            generateExpression(*literal.elements[i]);
            function_->i32StoreToMemory(offset);
            break;

        case TypeKind::Float:
            generateExpression(*literal.elements[i]);
            function_->f32StoreToMemory(offset);
            break;

        case TypeKind::Bool:
            // 0 - 1 = -1 (true), 0 - 0 = 0 (false)
            function_->i32Constant(0);
            generateExpression(*literal.elements[i]);
            function_->i32Subtraction();
            function_->i32StoreToMemory(offset);
            break;

        default:
            addError("Unsupported field element type in code generation");
            return;
        }
    }
}

bool CodeGenerator::reserveField(const Type &type, std::uint32_t &address, const std::string &context)
{
    if (!memory_.reserve(std::uint64_t(type.length) * FIELD_ELEMENT_BYTES, address))
    {
        addError("Field of length " + std::to_string(type.length) +
                 " does not fit in memory (" + context + ")");
        return false;
    }
    return true;
}

// memory.copy takes (destination, source, byte count)
// reserveField has already checked the byte count fits in 32 bits
void CodeGenerator::copyField(const Type &type)
{
    std::uint32_t bytes = type.length * FIELD_ELEMENT_BYTES;
    function_->i32Constant(static_cast<int>(bytes));
    function_->memoryCopy();
}

void CodeGenerator::generateFieldSource(const Expression &expression)
{
    if (const CallExpr *call = dynamic_cast<const CallExpr *>(&expression))
    {
        generateCall(*call);
        return;
    }

    generateExpression(expression);
}

// let x = <field> gives x its own block and fills it:
//   a literal is written straight into x's block
//   anything else is copied in, so x never shares memory with another value
void CodeGenerator::generateFieldLet(const LetStatement &statement)
{
    const Type &type = statement.resolvedType;

    std::uint32_t address = 0;
    if (!reserveField(type, address, "variable '" + statement.name + "'"))
    {
        return;
    }

    // the initializer is generated before x is declared, so in
    // "let x = [...]" any x inside the brackets is still the outer one
    if (const FieldLiteralExpr *literal =
            dynamic_cast<const FieldLiteralExpr *>(statement.initializer.get()))
    {
        writeFieldLiteral(*literal, address);
    }
    else if (statement.initializer != nullptr)
    {
        function_->i32Constant(static_cast<int>(address));
        generateFieldSource(*statement.initializer);
        copyField(type);
    }

    // the local holds the block's address for the rest of its scope
    std::string watName = declareLocal(statement.name, type);
    function_->i32Constant(static_cast<int>(address));
    function_->localSet(watName);
}

// x = <field> overwrites the data in x's block; x keeps the same address
// the new value is fully computed before the copy, and memory.copy handles
// overlapping memory, so x = x and values computed from x are safe
void CodeGenerator::generateFieldAssignment(const AssignmentStatement &assignment)
{
    function_->localGet(lookupLocal(assignment.name));
    generateFieldSource(*assignment.value);
    copyField(assignment.value->resolvedType);
}

// true if any statement in the block assigns to `name`
// shadowing is ignored, so this may say yes when an inner variable of the same name
// is assigned instead: that only costs an unneeded copy
static bool assignsTo(const Statement &statement, const std::string &name)
{
    if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        return assignment->name == name;
    }

    if (const BlockStatement *block = dynamic_cast<const BlockStatement *>(&statement))
    {
        for (const std::unique_ptr<Statement> &inner : block->statements)
        {
            if (assignsTo(*inner, name))
            {
                return true;
            }
        }
        return false;
    }

    if (const IfStatement *ifStatement = dynamic_cast<const IfStatement *>(&statement))
    {
        return (ifStatement->thenBlock != nullptr && assignsTo(*ifStatement->thenBlock, name)) ||
               (ifStatement->elseBranch != nullptr && assignsTo(*ifStatement->elseBranch, name));
    }

    if (const WhileStatement *whileStatement = dynamic_cast<const WhileStatement *>(&statement))
    {
        return whileStatement->body != nullptr && assignsTo(*whileStatement->body, name);
    }

    return false;
}

// a field parameter is the address of the caller's data
// reading it is safe, but assigning to it would overwrite the caller's variable,
// so a parameter the function assigns to is first copied into a block of its own
void CodeGenerator::copyAssignedFieldParameters(const std::vector<Parameter> &parameters,
                                                const BlockStatement *body)
{
    if (body == nullptr)
    {
        return;
    }

    for (const Parameter &parameter : parameters)
    {
        if (parameter.type.kind != TypeKind::Field || !assignsTo(*body, parameter.name))
        {
            continue;
        }

        std::uint32_t address = 0;
        if (!reserveField(parameter.type, address, "parameter '" + parameter.name + "'"))
        {
            continue;
        }

        function_->i32Constant(static_cast<int>(address));
        function_->localGet(parameter.name);
        copyField(parameter.type);

        function_->i32Constant(static_cast<int>(address));
        function_->localSet(parameter.name);
    }
}

// print(value) calls the host function for the value's type:
//   integer -> print_i32(value)      float -> print_f32(value)      bool -> print_bool(value)
//   field   -> print_field_<element>(address, length), the host reads the exported memory
void CodeGenerator::generatePrint(const CallExpr &call)
{
    if (call.arguments.size() != 1)
    {
        addError("print expects one argument; run semantic analysis first");
        return;
    }

    const Expression &argument = *call.arguments[0];
    const Type &type = argument.resolvedType;

    auto elementName = [](TypeKind kind) -> std::string {
        switch (kind)
        {
        case TypeKind::Int:
            return "i32";
        case TypeKind::Float:
            return "f32";
        case TypeKind::Bool:
            return "bool";
        default:
            return "";
        }
    };

    if (type.kind == TypeKind::Field)
    {
        std::string element = type.elementType == nullptr ? "" : elementName(type.elementType->kind);
        if (element.empty())
        {
            addError("print cannot print this field type");
            return;
        }

        // the field is read straight away, so no temporary copy is needed
        std::string hostFunction = "print_field_" + element;
        requireImport(hostFunction, {WatType::I32, WatType::I32});
        hostReadsMemory_ = true;
        generateFieldSource(argument);
        function_->i32Constant(static_cast<int>(type.length));
        function_->call("__" + hostFunction);
        return;
    }

    std::string element = elementName(type.kind);
    if (element.empty())
    {
        addError("print cannot print this type");
        return;
    }

    std::string hostFunction = "print_" + element;
    requireImport(hostFunction, {type.kind == TypeKind::Float ? WatType::F32 : WatType::I32});
    generateExpression(argument);
    function_->call("__" + hostFunction);
}

void CodeGenerator::generateCall(const CallExpr &call)
{
    if (call.name == "print")
    {
        generatePrint(call);
        return;
    }

    // arguments are pushed left to right, then the call consumes them
    for (const std::unique_ptr<Expression> &argument : call.arguments)
    {
        generateExpression(*argument);
    }
    function_->call(call.name);
}

void CodeGenerator::generateUnary(const UnaryExpr &unary)
{
    if (unary.op != UnaryOperator::Negate)
    {
        addError("Unsupported unary operator in code generation");
        return;
    }

    if (unary.operand->resolvedType.kind == TypeKind::Field)
    {
        addError("Field operations cannot be compiled yet (ruleset SIMD lowering is not implemented)");
        return;
    }

    switch (unary.operand->resolvedType.kind)
    {
    case TypeKind::Float:
        generateExpression(*unary.operand);
        function_->f32Negation();
        return;

    case TypeKind::Int:
        // WAT has no i32 negate, so compute 0 - operand
        function_->i32Constant(0);
        generateExpression(*unary.operand);
        function_->i32Subtraction();
        return;

    default:
        addError("Negation needs an int or float operand");
    }
}

void CodeGenerator::generateBinary(const BinaryExpr &binary)
{
    // checked before generating either side: in 2.0 * a the left side is a scalar,
    // so the scalar instructions below would otherwise be used on a field address
    if (binary.left->resolvedType.kind == TypeKind::Field ||
        binary.right->resolvedType.kind == TypeKind::Field)
    {
        addError("Field operations cannot be compiled yet (ruleset SIMD lowering is not implemented)");
        return;
    }

    generateExpression(*binary.left);
    generateExpression(*binary.right);

    // the instruction depends on the operand type not the result type
    TypeKind operandType = binary.left->resolvedType.kind;

    if (operandType == TypeKind::Int || operandType == TypeKind::Bool)
    {
        switch (binary.op)
        {
        case BinaryOperator::Add:
            function_->i32Addition();
            return;
        case BinaryOperator::Subtract:
            function_->i32Subtraction();
            return;
        case BinaryOperator::Multiply:
            function_->i32Multiplication();
            return;
        case BinaryOperator::Divide:
            function_->i32SignedDivision();
            return;
        case BinaryOperator::Equal:
            function_->i32Equal();
            return;
        case BinaryOperator::NotEqual:
            function_->i32NotEqual();
            return;
        case BinaryOperator::LessThan:
            function_->i32SignedLessThan();
            return;
        case BinaryOperator::GreaterThan:
            function_->i32SignedGreaterThan();
            return;
        case BinaryOperator::LessThanEqual:
            function_->i32SignedLessThanOrEqual();
            return;
        case BinaryOperator::GreaterThanEqual:
            function_->i32SignedGreaterThanOrEqual();
            return;
        }
    }
    else if (operandType == TypeKind::Float)
    {
        switch (binary.op)
        {
        case BinaryOperator::Add:
            function_->f32Addition();
            return;
        case BinaryOperator::Subtract:
            function_->f32Subtraction();
            return;
        case BinaryOperator::Multiply:
            function_->f32Multiplication();
            return;
        case BinaryOperator::Divide:
            function_->f32Division();
            return;
        case BinaryOperator::Equal:
            function_->f32Equal();
            return;
        case BinaryOperator::NotEqual:
            function_->f32NotEqual();
            return;
        case BinaryOperator::LessThan:
            function_->f32LessThan();
            return;
        case BinaryOperator::GreaterThan:
            function_->f32GreaterThan();
            return;
        case BinaryOperator::LessThanEqual:
            function_->f32LessThanOrEqual();
            return;
        case BinaryOperator::GreaterThanEqual:
            function_->f32GreaterThanOrEqual();
            return;
        }
    }

    addError("Unsupported operand type for a binary operator");
}
