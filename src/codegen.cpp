#include "codegen.hpp"

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
            generateFunction(*function);
            continue;
        }

        const RulesetDecl *ruleset =
            dynamic_cast<const RulesetDecl *>(declaration.get());

        if (ruleset != nullptr)
        {
            // TODO: IMPLEMENT RULESETS
            addError("Ruleset '" + ruleset->name +
                     "' cannot be compiled: ruleset code generation is not supported yet");
            continue;
        }

        addError("Unsupported declaration in code generation");
    }

    // every field has its address so the total size is known
    // exported so a JavaScript host can read and write field data
    if (!memory_.empty())
    {
        module_.setMemory(memory_.pages());
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


void CodeGenerator::generateFunction(const FunctionDecl &function)
{
    std::vector<WatParam> params;
    bool valid = true;

    for (const Parameter &parameter : function.parameters)
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
    if (function.returnType.kind != TypeKind::Void)
    {
        WatType type;
        if (toWatType(function.returnType, type, "return type of '" + function.name + "'"))
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

    function_ = &module_.addFunction(function.name, params, results);

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

    if (function.body != nullptr)
    {
        generateBlock(*function.body, false);

        // WAT checks that the stack holds the result at the end of the function
        if (function.returnType.kind != TypeKind::Void)
        {
            bool endsInReturn =
                !function.body->statements.empty() &&
                dynamic_cast<const ReturnStatement *>(function.body->statements.back().get()) != nullptr;

            if (!endsInReturn)
            {
                function_->unreachable();
            }
        }
    }

    exitScope();

    module_.exportFunction(function.name);
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
        // arguments are pushed left to right, then the call consumes them
        for (const std::unique_ptr<Expression> &argument : call->arguments)
        {
            generateExpression(*argument);
        }
        function_->call(call->name);
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
    if (!memory_.reserve(std::uint64_t(type.length) * FIELD_ELEMENT_BYTES, address))
    {
        addError("Field literal of length " + std::to_string(type.length) +
                 " does not fit in memory");
        return;
    }

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

    function_->i32Constant(static_cast<int>(address));
}

void CodeGenerator::generateUnary(const UnaryExpr &unary)
{
    if (unary.op != UnaryOperator::Negate)
    {
        addError("Unsupported unary operator in code generation");
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
