#include "codegen.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>

static bool assignsTo(const Statement &statement, const std::string &name);

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
                             function->returnType, function->body.get(), false);
            continue;
        }

        const RulesetDecl *ruleset =
            dynamic_cast<const RulesetDecl *>(declaration.get());

        if (ruleset != nullptr)
        {
            generateCallable(ruleset->name, ruleset->parameters,
                             ruleset->returnType, ruleset->body.get(), true);
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
    vector_ = VectorState();
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

// x then x_1, x_2, ... etc.
std::string CodeGenerator::uniqueLocalName(const std::string &name)
{
    std::string candidate = name;
    int suffix = 1;
    while (usedLocalNames_.count(candidate) != 0)
    {
        candidate = name + "_" + std::to_string(suffix++);
    }

    usedLocalNames_.insert(candidate);
    return candidate;
}

// declares a local and returns the unique WAT name chosen for it
std::string CodeGenerator::declareLocal(const std::string &name, const Type &type)
{
    WatType watType;
    if (!toWatType(type, watType, "variable '" + name + "'"))
    {
        return name;
    }

    return declareLocal(name, watType);
}

std::string CodeGenerator::declareLocal(const std::string &name, WatType type)
{
    std::string candidate = uniqueLocalName(name);
    scopes_.back()[name] = candidate;
    function_->addLocal(candidate, type);
    return candidate;
}

// user names cannot start with `__` so will never clash with a variable
std::string CodeGenerator::newLocal(const std::string &base, WatType type)
{
    std::string candidate = uniqueLocalName("__" + base);
    function_->addLocal(candidate, type);
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


// reductions combine every element of a field into one uniform
static bool isReductionName(const std::string &name)
{
    return name == "sum" || name == "min_of" || name == "max_of" ||
           name == "any" || name == "all";
}

// true if the expression contains an element-wise field operation (a + b, -a, a < 2.0)
// or a reduction
static bool usesFieldOperations(const Expression &expression)
{
    if (const BinaryExpr *binary = dynamic_cast<const BinaryExpr *>(&expression))
    {
        return binary->resolvedType.kind == TypeKind::Field ||
               binary->left->resolvedType.kind == TypeKind::Field ||
               usesFieldOperations(*binary->left) || usesFieldOperations(*binary->right);
    }

    if (const UnaryExpr *unary = dynamic_cast<const UnaryExpr *>(&expression))
    {
        return unary->resolvedType.kind == TypeKind::Field || usesFieldOperations(*unary->operand);
    }

    if (const CallExpr *call = dynamic_cast<const CallExpr *>(&expression))
    {
        if (isReductionName(call->name))
        {
            return true;
        }

        for (const std::unique_ptr<Expression> &argument : call->arguments)
        {
            if (usesFieldOperations(*argument))
            {
                return true;
            }
        }
        return false;
    }

    if (const FieldLiteralExpr *literal = dynamic_cast<const FieldLiteralExpr *>(&expression))
    {
        for (const std::unique_ptr<Expression> &element : literal->elements)
        {
            if (usesFieldOperations(*element))
            {
                return true;
            }
        }
    }

    return false;
}

// the expressions directly inside a simple statement (let, assignment, expression, return)
static const Expression *statementExpression(const Statement &statement)
{
    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        return let->initializer.get();
    }
    if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        return assignment->value.get();
    }
    if (const ExpressionStatement *expression = dynamic_cast<const ExpressionStatement *>(&statement))
    {
        return expression->expression.get();
    }
    if (const ReturnStatement *returnStatement = dynamic_cast<const ReturnStatement *>(&statement))
    {
        return returnStatement->value.get();
    }
    return nullptr;
}

// a let, assignment, expression or return whose value is not a field
static bool isScalarStatement(const Statement &statement)
{
    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        return let->resolvedType.kind != TypeKind::Field;
    }

    bool simple = dynamic_cast<const AssignmentStatement *>(&statement) != nullptr ||
                  dynamic_cast<const ExpressionStatement *>(&statement) != nullptr ||
                  dynamic_cast<const ReturnStatement *>(&statement) != nullptr;
    if (!simple)
    {
        return false;
    }

    const Expression *expression = statementExpression(statement);
    return expression == nullptr || expression->resolvedType.kind != TypeKind::Field;
}

// visits an expression and everything inside it
static void visitExpression(const Expression &expression,
                            const std::function<void(const Expression &)> &visit)
{
    visit(expression);

    if (const BinaryExpr *binary = dynamic_cast<const BinaryExpr *>(&expression))
    {
        visitExpression(*binary->left, visit);
        visitExpression(*binary->right, visit);
    }
    else if (const UnaryExpr *unary = dynamic_cast<const UnaryExpr *>(&expression))
    {
        visitExpression(*unary->operand, visit);
    }
    else if (const CallExpr *call = dynamic_cast<const CallExpr *>(&expression))
    {
        for (const std::unique_ptr<Expression> &argument : call->arguments)
        {
            visitExpression(*argument, visit);
        }
    }
    else if (const FieldLiteralExpr *literal = dynamic_cast<const FieldLiteralExpr *>(&expression))
    {
        for (const std::unique_ptr<Expression> &element : literal->elements)
        {
            visitExpression(*element, visit);
        }
    }
}

static bool containsReduction(const Expression &expression)
{
    bool found = false;
    visitExpression(expression, [&](const Expression &inner) {
        const CallExpr *call = dynamic_cast<const CallExpr *>(&inner);
        found = found || (call != nullptr && isReductionName(call->name));
    });
    return found;
}

static bool containsReduction(const Statement &statement)
{
    const Expression *expression = statementExpression(statement);
    return expression != nullptr && containsReduction(*expression);
}

// the reductions in a statement, outermost first (nested ones are rejected later,
// when the outer argument's uniforms are prepared)
static void forEachReduction(const Statement &statement, std::vector<const CallExpr *> &reductions)
{
    const Expression *expression = statementExpression(statement);
    if (expression == nullptr)
    {
        return;
    }

    visitExpression(*expression, [&](const Expression &inner) {
        const CallExpr *call = dynamic_cast<const CallExpr *>(&inner);
        if (call != nullptr && isReductionName(call->name))
        {
            reductions.push_back(call);
        }
    });
}

static void forEachVariable(const Expression &expression, std::vector<const VariableExpr *> &variables)
{
    visitExpression(expression, [&](const Expression &inner) {
        if (const VariableExpr *variable = dynamic_cast<const VariableExpr *>(&inner))
        {
            variables.push_back(variable);
        }
    });
}

static bool usesFieldOperations(const Statement &statement)
{
    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        return let->initializer != nullptr && usesFieldOperations(*let->initializer);
    }

    if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        return usesFieldOperations(*assignment->value);
    }

    if (const ReturnStatement *returnStatement = dynamic_cast<const ReturnStatement *>(&statement))
    {
        return returnStatement->value != nullptr && usesFieldOperations(*returnStatement->value);
    }

    if (const ExpressionStatement *expression = dynamic_cast<const ExpressionStatement *>(&statement))
    {
        return expression->expression != nullptr && usesFieldOperations(*expression->expression);
    }

    if (const BlockStatement *block = dynamic_cast<const BlockStatement *>(&statement))
    {
        for (const std::unique_ptr<Statement> &inner : block->statements)
        {
            if (usesFieldOperations(*inner))
            {
                return true;
            }
        }
        return false;
    }

    if (const IfStatement *ifStatement = dynamic_cast<const IfStatement *>(&statement))
    {
        return usesFieldOperations(*ifStatement->condition) ||
               (ifStatement->thenBlock != nullptr && usesFieldOperations(*ifStatement->thenBlock)) ||
               (ifStatement->elseBranch != nullptr && usesFieldOperations(*ifStatement->elseBranch));
    }

    if (const WhileStatement *whileStatement = dynamic_cast<const WhileStatement *>(&statement))
    {
        return usesFieldOperations(*whileStatement->condition) ||
               (whileStatement->body != nullptr && usesFieldOperations(*whileStatement->body));
    }

    return false;
}

// float fields use f32x4 lanes, integer fields and bool masks use i32x4
static LaneType laneOf(TypeKind element)
{
    return element == TypeKind::Float ? LaneType::F32x4 : LaneType::I32x4;
}

// the lane type of a value inside the loop
static LaneType laneOf(const Type &type)
{
    if (type.kind == TypeKind::Field && type.elementType != nullptr)
    {
        return laneOf(type.elementType->kind);
    }
    return laneOf(type.kind);
}

void CodeGenerator::vectorError(const std::string &reason)
{
    addError("Ruleset '" + vector_.ruleset + "' cannot be vectorised: " + reason);
}

// the loop has one chunk count so every field it touches must have the same length
void CodeGenerator::checkVectorLength(std::uint32_t length)
{
    if (vector_.length == 0)
    {
        vector_.length = length;
    }
    else if (vector_.length != length && !vector_.lengthMismatchReported)
    {
        vector_.lengthMismatchReported = true;
        vectorError("fields of different lengths (" + std::to_string(vector_.length) +
                    " and " + std::to_string(length) + ") are used together");
    }
}

// a field parameter is read in the loop through a v128 local that is loaded at the
// start of every chunk; assigning to the parameter only changes that local, so the
// caller's field is never written
std::string CodeGenerator::parameterChunk(const std::string &parameter)
{
    for (const auto &[name, chunk] : vector_.parameterChunks)
    {
        if (name == parameter)
        {
            return chunk;
        }
    }

    std::string chunk = newLocal(parameter, WatType::V128);
    vector_.parameterChunks.emplace_back(parameter, chunk);
    return chunk;
}

// a ruleset with field operations becomes one fused loop over its fields, 4 elements
// (one v128) at a time. e.g. with x: field<f32, 8> and k: f32
//
//   let y = x * k;          k is splatted once:   local.get $k  f32x4.splat  local.set $__splat
//   return y + 1.0;         1.0 likewise
//                           loop $chunk_loop      (offset = 0, 16)
//                             x + offset  v128.load  local.set $__x
//                             local.get $__x  local.get $__splat  f32x4.mul  local.set $y
//                             result + offset  (y + 1.0)  v128.store
//                             offset += 16, repeat while offset < 32
//                           end
//                           i32.const <result>  return
//
// scalar statements run before the loop in order, so a uniform used by a field
// statement is splatted with its value at that point in the body
//
// reductions (sum, min_of, max_of, any, all) are only known once the loop has seen
// every chunk, so the body is split at the first scalar statement that uses one:
//   prefix: as above, before and inside the loop
//   tail:   scalar statements only, generated after the loop in order, where each
//           reduction reads the result its accumulator produced
void CodeGenerator::generateVectorisedRuleset(const std::string &name,
                                              const std::vector<Parameter> &parameters,
                                              const Type &returnType,
                                              const BlockStatement &body)
{
    vector_ = VectorState();
    vector_.active = true;
    vector_.ruleset = name;

    for (const Parameter &parameter : parameters)
    {
        if (parameter.type.kind == TypeKind::Field && assignsTo(body, parameter.name))
        {
            vector_.assignedParameters.insert(parameter.name);
        }
    }

    if (returnType.kind == TypeKind::Field)
    {
        checkVectorLength(returnType.length);
        if (!reserveField(returnType, vector_.result, "result of '" + name + "'"))
        {
            vector_ = VectorState();
            return;
        }
    }

    const std::size_t count = body.statements.size();
    std::size_t tailStart = count;
    for (std::size_t i = 0; i < count; i++)
    {
        if (isScalarStatement(*body.statements[i]) && containsReduction(*body.statements[i]))
        {
            tailStart = i;
            break;
        }
    }

    // pass 1: everything that happens once, before the loop
    const ReturnStatement *finalReturn = nullptr;
    std::size_t errorsBefore = errors_.size();
    for (std::size_t i = 0; i < count; i++)
    {
        bool isLast = i + 1 == count;
        bool prepared = i < tailStart ? prepareVectorStatement(*body.statements[i], isLast)
                                      : prepareTailStatement(*body.statements[i], isLast);
        if (!prepared)
        {
            break;
        }
        if (isLast)
        {
            finalReturn = dynamic_cast<const ReturnStatement *>(body.statements[i].get());
        }
    }

    if (errors_.size() != errorsBefore)
    {
        vector_ = VectorState();
        return;
    }

    // pass 2: the loop, one iteration per chunk. locals start at zero, so the offset does too
    if (vector_.length != 0)
    {
        vector_.offset = newLocal("offset", WatType::I32);
        const std::string &offset = vector_.offset;
        std::uint32_t end = (vector_.length + 3) / 4 * CHUNK_BYTES;

        // when the length is not a multiple of 4, the last chunk has padding lanes,
        // which reductions must ignore. lane k is real if offset + 4k < length * 4
        std::string valid;
        if (!vector_.reductions.empty() && vector_.length % 4 != 0)
        {
            valid = newLocal("valid", WatType::V128);
        }

        function_->beginLoop("chunk_loop");

        for (const auto &[parameter, chunk] : vector_.parameterChunks)
        {
            function_->localGet(parameter);
            function_->localGet(offset);
            function_->i32Addition();
            function_->v128LoadFromMemory();
            function_->localSet(chunk);
        }

        if (!valid.empty())
        {
            int bytes = static_cast<int>(vector_.length * FIELD_ELEMENT_BYTES);
            function_->v128ConstantI32({0, 4, 8, 12});
            function_->localGet(offset);
            function_->vectorSplat(LaneType::I32x4);
            function_->vectorAddition(LaneType::I32x4);
            function_->v128ConstantI32({bytes, bytes, bytes, bytes});
            function_->vectorLessThan(LaneType::I32x4);
            function_->localSet(valid);
        }

        for (std::size_t i = 0; i < tailStart; i++)
        {
            const Statement &statement = *body.statements[i];
            const ReturnStatement *returnStatement = dynamic_cast<const ReturnStatement *>(&statement);
            if (returnStatement != nullptr && returnType.kind == TypeKind::Field)
            {
                function_->i32Constant(static_cast<int>(vector_.result));
                function_->localGet(offset);
                function_->i32Addition();
                generateVectorExpression(*returnStatement->value);
                function_->v128StoreToMemory();
                continue;
            }

            generateVectorStatement(statement);
        }

        // acc = acc <combine> chunk, with padding lanes replaced by the identity
        for (const Reduction &reduction : vector_.reductions)
        {
            function_->localGet(reduction.accumulator);
            generateVectorExpression(*reduction.call->arguments[0]);
            if (!valid.empty())
            {
                pushReductionIdentity(reduction);
                function_->localGet(valid);
                function_->v128BitSelect();
            }
            combineReduction(reduction);
            function_->localSet(reduction.accumulator);
        }

        function_->localGet(offset);
        function_->i32Constant(static_cast<int>(CHUNK_BYTES));
        function_->i32Addition();
        function_->localTee(offset);
        function_->i32Constant(static_cast<int>(end));
        function_->i32SignedLessThan();
        function_->branchIf("chunk_loop");
        function_->end();
    }

    for (const Reduction &reduction : vector_.reductions)
    {
        finishReduction(reduction);
    }

    if (tailStart < count)
    {
        for (std::size_t i = tailStart; i < count; i++)
        {
            generateStatement(*body.statements[i]);
        }
    }
    // the result: a field is the block the loop wrote, a scalar is computed after the loop
    else if (finalReturn != nullptr)
    {
        if (returnType.kind == TypeKind::Field)
        {
            function_->i32Constant(static_cast<int>(vector_.result));
        }
        else if (finalReturn->value != nullptr)
        {
            generateExpression(*finalReturn->value);
        }
        function_->returnFromFunction();
    }

    vector_ = VectorState();
}

// pass 1 for a statement after the first reduction: it runs after the loop, so it can
// only work with uniforms. its reductions get accumulators in the loop
bool CodeGenerator::prepareTailStatement(const Statement &statement, bool isLast)
{
    bool simple = dynamic_cast<const LetStatement *>(&statement) != nullptr ||
                  dynamic_cast<const AssignmentStatement *>(&statement) != nullptr ||
                  dynamic_cast<const ExpressionStatement *>(&statement) != nullptr ||
                  dynamic_cast<const ReturnStatement *>(&statement) != nullptr;
    if (!simple)
    {
        return prepareVectorStatement(statement, isLast); // reports the error
    }

    if (!isScalarStatement(statement))
    {
        vectorError("field operations cannot come after a reduction (they would need a second loop)");
        return false;
    }

    if (dynamic_cast<const ReturnStatement *>(&statement) != nullptr && !isLast)
    {
        vectorError("return must be the last statement");
        return false;
    }

    std::vector<const CallExpr *> reductions;
    forEachReduction(statement, reductions);
    for (const CallExpr *call : reductions)
    {
        prepareReduction(*call);
    }

    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        vector_.tailNames.insert(let->name);
    }
    else if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        vector_.tailNames.insert(assignment->name);
    }
    return true;
}

// the reduction's argument is evaluated in the loop, so its uniforms are splatted
// before the loop: they cannot be values that the tail computes after it
void CodeGenerator::prepareReduction(const CallExpr &call)
{
    const Expression &argument = *call.arguments[0];

    std::vector<const VariableExpr *> variables;
    forEachVariable(argument, variables);
    for (const VariableExpr *variable : variables)
    {
        if (vector_.tailNames.count(variable->name) != 0)
        {
            vectorError("the argument of '" + call.name + "' uses '" + variable->name +
                        "', which is set after the first reduction");
            return;
        }
    }

    prepareVectorExpression(argument);

    Reduction reduction;
    reduction.call = &call;
    reduction.element = argument.resolvedType.elementType->kind;
    reduction.accumulator = newLocal(call.name, WatType::V128);
    reduction.result = newLocal(call.name + "_result",
                                reduction.element == TypeKind::Float ? WatType::F32 : WatType::I32);

    pushReductionIdentity(reduction);
    function_->localSet(reduction.accumulator);

    vector_.reductions.push_back(reduction);
    vector_.reductionResults[&call] = reduction.result;
}

// the value that leaves the accumulator unchanged: x + 0, min(x, +inf), x and all-ones, ...
void CodeGenerator::pushReductionIdentity(const Reduction &reduction)
{
    const std::string &name = reduction.call->name;
    bool isFloat = reduction.element == TypeKind::Float;
    const float infinity = std::numeric_limits<float>::infinity();
    const int largest = std::numeric_limits<int>::max();
    const int smallest = std::numeric_limits<int>::min();

    if (name == "min_of")
    {
        isFloat ? function_->v128ConstantF32({infinity, infinity, infinity, infinity})
                : function_->v128ConstantI32({largest, largest, largest, largest});
    }
    else if (name == "max_of")
    {
        isFloat ? function_->v128ConstantF32({-infinity, -infinity, -infinity, -infinity})
                : function_->v128ConstantI32({smallest, smallest, smallest, smallest});
    }
    else if (name == "all")
    {
        function_->v128ConstantI32({-1, -1, -1, -1});
    }
    else // sum and any
    {
        function_->v128ConstantI32({0, 0, 0, 0});
    }
}

// pops two v128s and pushes them combined lane by lane
void CodeGenerator::combineReduction(const Reduction &reduction)
{
    const std::string &name = reduction.call->name;
    LaneType lanes = laneOf(reduction.element);

    if (name == "sum")
    {
        function_->vectorAddition(lanes);
    }
    else if (name == "min_of")
    {
        function_->vectorMinimum(lanes);
    }
    else if (name == "max_of")
    {
        function_->vectorMaximum(lanes);
    }
    else if (name == "any")
    {
        function_->v128Or();
    }
    else
    {
        function_->v128And();
    }
}

// after the loop, the 4 lanes of the accumulator are combined into one scalar
// any / all have instructions for this; the others fold the lanes with two shuffles:
//   [a b c d] with [c d a b]  ->  [ac bd ac bd]
//   that with [bd ac bd ac]   ->  every lane holds the whole result, take lane 0
void CodeGenerator::finishReduction(const Reduction &reduction)
{
    const std::string &name = reduction.call->name;
    const std::string &accumulator = reduction.accumulator;

    if (name == "any" || name == "all")
    {
        function_->localGet(accumulator);
        name == "any" ? function_->v128AnyTrue() : function_->i32x4AllTrue();
        function_->localSet(reduction.result);
        return;
    }

    // the shuffle pops two operands (both acc), so acc is pushed a third time to combine with
    function_->localGet(accumulator);
    function_->localGet(accumulator);
    function_->localGet(accumulator);
    function_->vectorShuffle({2, 3, 0, 1});
    combineReduction(reduction);
    function_->localTee(accumulator);
    function_->localGet(accumulator);
    function_->localGet(accumulator);
    function_->vectorShuffle({1, 0, 3, 2});
    combineReduction(reduction);
    function_->vectorExtractLane(laneOf(reduction.element), 0);
    function_->localSet(reduction.result);
}

// pass 1 for one statement; returns false if the statement cannot be vectorised
bool CodeGenerator::prepareVectorStatement(const Statement &statement, bool isLast)
{
    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        if (let->resolvedType.kind != TypeKind::Field)
        {
            generateLet(*let);
            return true;
        }

        checkVectorLength(let->resolvedType.length);
        if (let->initializer != nullptr)
        {
            prepareVectorExpression(*let->initializer);
        }

        // declared after the initializer, so "let y = y * 2.0" reads the outer y
        std::string watName = declareLocal(let->name, WatType::V128);
        vector_.vectorLocals.insert(watName);
        vector_.targets[let] = watName;
        return true;
    }

    if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        if (assignment->value->resolvedType.kind != TypeKind::Field)
        {
            generateStatement(*assignment);
            return true;
        }

        prepareVectorExpression(*assignment->value);

        std::string watName = lookupLocal(assignment->name);
        vector_.targets[assignment] =
            vector_.vectorLocals.count(watName) != 0 ? watName : parameterChunk(watName);
        return true;
    }

    if (const ExpressionStatement *expression = dynamic_cast<const ExpressionStatement *>(&statement))
    {
        if (expression->expression == nullptr)
        {
            return true;
        }

        if (expression->expression->resolvedType.kind != TypeKind::Field)
        {
            generateStatement(*expression);
            return true;
        }

        prepareVectorExpression(*expression->expression);
        return true;
    }

    if (const ReturnStatement *returnStatement = dynamic_cast<const ReturnStatement *>(&statement))
    {
        if (!isLast)
        {
            vectorError("return must be the last statement");
            return false;
        }

        // a scalar result is generated after the loop
        if (returnStatement->value != nullptr &&
            returnStatement->value->resolvedType.kind == TypeKind::Field)
        {
            prepareVectorExpression(*returnStatement->value);
        }
        return true;
    }

    if (dynamic_cast<const IfStatement *>(&statement) != nullptr)
    {
        vectorError("if statements are not supported in rulesets with field operations");
    }
    else if (dynamic_cast<const WhileStatement *>(&statement) != nullptr)
    {
        vectorError("while loops are not supported in rulesets with field operations");
    }
    else if (dynamic_cast<const BlockStatement *>(&statement) != nullptr)
    {
        vectorError("nested blocks are not supported in rulesets with field operations");
    }
    else
    {
        vectorError("unsupported statement");
    }
    return false;
}

// pass 1 for a field expression: finds the parts that are evaluated once before the loop
void CodeGenerator::prepareVectorExpression(const Expression &expression)
{
    const Type &type = expression.resolvedType;

    // a uniform (scalar) operand is computed now and splatted to every lane
    // bools become masks first: 0 - b gives -1 for true and 0 for false
    if (type.kind != TypeKind::Field)
    {
        // a reduction is only known after the loop, but this uniform is needed inside it
        if (containsReduction(expression))
        {
            vectorError("the result of a reduction cannot be used by field operations "
                        "in the same ruleset (it would need a second loop)");
            return;
        }

        if (type.kind == TypeKind::Bool)
        {
            function_->i32Constant(0);
            generateExpression(expression);
            function_->i32Subtraction();
        }
        else
        {
            generateExpression(expression);
        }

        function_->vectorSplat(laneOf(type.kind));
        std::string splat = newLocal("splat", WatType::V128);
        function_->localSet(splat);
        vector_.splats[&expression] = splat;
        return;
    }

    checkVectorLength(type.length);

    if (const BinaryExpr *binary = dynamic_cast<const BinaryExpr *>(&expression))
    {
        prepareVectorExpression(*binary->left);
        prepareVectorExpression(*binary->right);
        return;
    }

    if (const UnaryExpr *unary = dynamic_cast<const UnaryExpr *>(&expression))
    {
        prepareVectorExpression(*unary->operand);
        return;
    }

    if (const VariableExpr *variable = dynamic_cast<const VariableExpr *>(&expression))
    {
        std::string watName = lookupLocal(variable->name);
        vector_.variables[variable] =
            vector_.vectorLocals.count(watName) != 0 ? watName : parameterChunk(watName);
        return;
    }

    // a literal or a call gives the address of a whole field in memory,
    // which the loop reads one chunk at a time
    if (dynamic_cast<const FieldLiteralExpr *>(&expression) != nullptr ||
        dynamic_cast<const CallExpr *>(&expression) != nullptr)
    {
        generateExpression(expression);
        std::string address = newLocal("field", WatType::I32);
        function_->localSet(address);
        vector_.addresses[&expression] = address;
        return;
    }

    vectorError("unsupported field expression");
}

// pass 2 for one statement, inside the loop: only field values do work here
void CodeGenerator::generateVectorStatement(const Statement &statement)
{
    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        if (let->resolvedType.kind == TypeKind::Field && let->initializer != nullptr)
        {
            generateVectorExpression(*let->initializer);
            function_->localSet(vector_.targets.at(let));
        }
        return;
    }

    if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        if (assignment->value->resolvedType.kind == TypeKind::Field)
        {
            generateVectorExpression(*assignment->value);
            function_->localSet(vector_.targets.at(assignment));
        }
        return;
    }

    // a field expression statement has no effect on the chunk (its calls already ran)
}

// pass 2 for a field expression: pushes the v128 for the current chunk
void CodeGenerator::generateVectorExpression(const Expression &expression)
{
    auto splat = vector_.splats.find(&expression);
    if (splat != vector_.splats.end())
    {
        function_->localGet(splat->second);
        return;
    }

    auto address = vector_.addresses.find(&expression);
    if (address != vector_.addresses.end())
    {
        function_->localGet(address->second);
        function_->localGet(vector_.offset);
        function_->i32Addition();
        function_->v128LoadFromMemory();
        return;
    }

    auto variable = vector_.variables.find(&expression);
    if (variable != vector_.variables.end())
    {
        function_->localGet(variable->second);
        return;
    }

    if (const UnaryExpr *unary = dynamic_cast<const UnaryExpr *>(&expression))
    {
        generateVectorExpression(*unary->operand);
        function_->vectorNegation(laneOf(unary->operand->resolvedType));
        return;
    }

    const BinaryExpr *binary = dynamic_cast<const BinaryExpr *>(&expression);
    if (binary == nullptr)
    {
        vectorError("unsupported field expression");
        return;
    }

    generateVectorExpression(*binary->left);
    generateVectorExpression(*binary->right);

    // the lanes follow the operands (a comparison of floats is f32x4 even though
    // it produces a bool mask); a uniform operand was splatted with the same lanes
    LaneType lanes = laneOf(binary->left->resolvedType);

    switch (binary->op)
    {
    case BinaryOperator::Add:
        function_->vectorAddition(lanes);
        return;
    case BinaryOperator::Subtract:
        function_->vectorSubtraction(lanes);
        return;
    case BinaryOperator::Multiply:
        function_->vectorMultiplication(lanes);
        return;
    case BinaryOperator::Divide: // semantic analysis allows float fields only
        function_->f32x4Division();
        return;
    case BinaryOperator::Equal:
        function_->vectorEqual(lanes);
        return;
    case BinaryOperator::NotEqual:
        function_->vectorNotEqual(lanes);
        return;
    case BinaryOperator::LessThan:
        function_->vectorLessThan(lanes);
        return;
    case BinaryOperator::GreaterThan:
        function_->vectorGreaterThan(lanes);
        return;
    case BinaryOperator::LessThanEqual:
        function_->vectorLessThanOrEqual(lanes);
        return;
    case BinaryOperator::GreaterThanEqual:
        function_->vectorGreaterThanOrEqual(lanes);
        return;
    }
}

// functions and rulesets compile the same way: a WAT function with the same name,
// exported so a JavaScript host can call it. field parameters and results are
// i32 addresses (see generateFieldLet / generateCall for how they are copied)
void CodeGenerator::generateCallable(const std::string &name,
                                     const std::vector<Parameter> &parameters,
                                     const Type &returnType,
                                     const BlockStatement *body,
                                     bool isRuleset)
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

    if (isRuleset && body != nullptr && usesFieldOperations(*body))
    {
        generateVectorisedRuleset(name, parameters, returnType, *body);
    }
    else if (body != nullptr)
    {
        copyAssignedFieldParameters(parameters, body);

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

// blocks are padded to a whole number of 16 byte chunks, so a vectorised ruleset
// can load and store full v128s without a scalar tail: field<f32, 6> takes 32 bytes,
// and lanes past the length hold unused values that nothing reads
bool CodeGenerator::reserveField(const Type &type, std::uint32_t &address, const std::string &context)
{
    std::uint64_t bytes = std::uint64_t(type.length) * FIELD_ELEMENT_BYTES;
    bytes = (bytes + CHUNK_BYTES - 1) / CHUNK_BYTES * CHUNK_BYTES;

    if (!memory_.reserve(bytes, address))
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
    // the length is part of the field's type, so the argument is not evaluated
    // (like sizeof in C, any calls inside it do not run)
    if (call.name == "length")
    {
        const Type &type = call.arguments.empty() ? Type{} : call.arguments[0]->resolvedType;
        if (type.kind != TypeKind::Field)
        {
            addError("length expects a field argument; run semantic analysis first");
            return;
        }
        function_->i32Constant(static_cast<int>(type.length));
        return;
    }

    // a reduction's result was computed by the loop (see finishReduction)
    if (isReductionName(call.name))
    {
        auto result = vector_.reductionResults.find(&call);
        if (result == vector_.reductionResults.end())
        {
            addError("Reduction '" + call.name + "' outside a vectorised ruleset; run semantic analysis first");
            return;
        }
        function_->localGet(result->second);
        return;
    }

    // in a vectorised ruleset, calls run before the loop on whole fields in memory,
    // so a field argument cannot be a value that only exists chunk by chunk in the loop
    if (vector_.active)
    {
        for (const std::unique_ptr<Expression> &argument : call.arguments)
        {
            if (argument->resolvedType.kind != TypeKind::Field ||
                dynamic_cast<const FieldLiteralExpr *>(argument.get()) != nullptr ||
                dynamic_cast<const CallExpr *>(argument.get()) != nullptr)
            {
                continue;
            }

            const VariableExpr *variable = dynamic_cast<const VariableExpr *>(argument.get());
            if (variable != nullptr)
            {
                std::string watName = lookupLocal(variable->name);
                if (vector_.vectorLocals.count(watName) == 0 &&
                    vector_.assignedParameters.count(watName) == 0)
                {
                    continue;
                }
            }

            vectorError("a field passed to '" + call.name +
                        "' must be a literal, a call, or a parameter that is never assigned");
            return;
        }
    }

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

    // field operations are compiled by generateVectorExpression
    if (unary.operand->resolvedType.kind == TypeKind::Field)
    {
        addError("Field operation outside a vectorised ruleset; run semantic analysis first");
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
    // field operations are compiled by generateVectorExpression
    // (checked before generating either side: in 2.0 * a the left side is a scalar)
    if (binary.left->resolvedType.kind == TypeKind::Field ||
        binary.right->resolvedType.kind == TypeKind::Field)
    {
        addError("Field operation outside a vectorised ruleset; run semantic analysis first");
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
