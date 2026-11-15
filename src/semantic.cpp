#include <functional>
#include <unordered_map>
#include <unordered_set>
#include "semantic.hpp"

// goes through the program and checks its declarations
bool SemanticAnalyser::analyse(const Program &program)
{
    // reset the symbol table and errors before checking a program
    // this prevents declarations and errors from previous runs carrying over
    symbols_ = SymbolTable();
    errors_.clear();

    // pass 1: register all functions and rulesets
    for (const std::unique_ptr<Declaration> &declaration : program.declarations)
    {
        // check if the current declaration is a normal function
        // dynamic_cast returns nullptr if it is not a FunctionDecl
        const FunctionDecl *function =
            dynamic_cast<const FunctionDecl *>(declaration.get());

        if (function != nullptr)
        {
            // register the function in the symbol table
            registerFunction(*function);

            // move to the next declaration
            continue;
        }

        // check if the declaration is a ruleset instead
        const RulesetDecl *ruleset =
            dynamic_cast<const RulesetDecl *>(declaration.get());

        if (ruleset != nullptr)
        {
            // register the ruleset in the symbol table
            registerRuleset(*ruleset);
        }
    }

    // pass 2: analyse function and ruleset bodies

    // keep track of names already analysed
    std::unordered_set<std::string> analysedNames;

    for (const std::unique_ptr<Declaration> &declaration : program.declarations)
    {
        // check if this declaration is a function
        const FunctionDecl *function =
            dynamic_cast<const FunctionDecl *>(declaration.get());

        if (function != nullptr)
        {
            // only analyse the first function with this name
            if (analysedNames.insert(function->name).second)
            {
                analyseFunction(*function);
            }

            continue;
        }

        // check if this declaration is a ruleset
        const RulesetDecl *ruleset =
            dynamic_cast<const RulesetDecl *>(declaration.get());

        if (ruleset != nullptr)
        {
            // only analyse the first ruleset with this name
            if (analysedNames.insert(ruleset->name).second)
            {
                analyseRuleset(*ruleset);
            }
        }
    }

    checkRecursiveFieldUse(program);

    // return true if no semantic errors were found
    return errors_.empty();
}

// calls `visit` on every expression inside the statement, including sub-expressions
static void forEachExpression(const Expression &expression,
                              const std::function<void(const Expression &)> &visit)
{
    visit(expression);

    if (const BinaryExpr *binary = dynamic_cast<const BinaryExpr *>(&expression))
    {
        forEachExpression(*binary->left, visit);
        forEachExpression(*binary->right, visit);
    }
    else if (const UnaryExpr *unary = dynamic_cast<const UnaryExpr *>(&expression))
    {
        forEachExpression(*unary->operand, visit);
    }
    else if (const CallExpr *call = dynamic_cast<const CallExpr *>(&expression))
    {
        for (const std::unique_ptr<Expression> &argument : call->arguments)
        {
            forEachExpression(*argument, visit);
        }
    }
    else if (const FieldLiteralExpr *field = dynamic_cast<const FieldLiteralExpr *>(&expression))
    {
        for (const std::unique_ptr<Expression> &element : field->elements)
        {
            forEachExpression(*element, visit);
        }
    }
}

static void forEachExpression(const Statement &statement,
                              const std::function<void(const Expression &)> &visit)
{
    // statements can hold null pieces after a parse of an incomplete program
    auto visitExpression = [&](const std::unique_ptr<Expression> &expression) {
        if (expression != nullptr)
        {
            forEachExpression(*expression, visit);
        }
    };

    if (const LetStatement *let = dynamic_cast<const LetStatement *>(&statement))
    {
        visitExpression(let->initializer);
    }
    else if (const ReturnStatement *returnStatement = dynamic_cast<const ReturnStatement *>(&statement))
    {
        visitExpression(returnStatement->value);
    }
    else if (const ExpressionStatement *expressionStatement = dynamic_cast<const ExpressionStatement *>(&statement))
    {
        visitExpression(expressionStatement->expression);
    }
    else if (const AssignmentStatement *assignment = dynamic_cast<const AssignmentStatement *>(&statement))
    {
        visitExpression(assignment->value);
    }
    else if (const BlockStatement *block = dynamic_cast<const BlockStatement *>(&statement))
    {
        for (const std::unique_ptr<Statement> &inner : block->statements)
        {
            forEachExpression(*inner, visit);
        }
    }
    else if (const IfStatement *ifStatement = dynamic_cast<const IfStatement *>(&statement))
    {
        visitExpression(ifStatement->condition);
        if (ifStatement->thenBlock != nullptr)
        {
            forEachExpression(*ifStatement->thenBlock, visit);
        }
        if (ifStatement->elseBranch != nullptr)
        {
            forEachExpression(*ifStatement->elseBranch, visit);
        }
    }
    else if (const WhileStatement *whileStatement = dynamic_cast<const WhileStatement *>(&statement))
    {
        visitExpression(whileStatement->condition);
        if (whileStatement->body != nullptr)
        {
            forEachExpression(*whileStatement->body, visit);
        }
    }
}

// what the recursion check needs to know about one function or ruleset
struct CallGraphNode
{
    std::string name;
    std::unordered_set<std::string> callees;  // functions and rulesets it calls directly
    bool usesFields = false;                  // any field in its signature or body
};

// works for FunctionDecl and RulesetDecl, which have the same members
// (field variables, literals, calls returning fields and field arguments all count as field use)
template <typename Declaration>
static CallGraphNode describeCallable(const Declaration &declaration)
{
    CallGraphNode node;
    node.name = declaration.name;
    node.usesFields = declaration.returnType.kind == TypeKind::Field;

    for (const Parameter &parameter : declaration.parameters)
    {
        if (parameter.type.kind == TypeKind::Field)
        {
            node.usesFields = true;
        }
    }

    if (declaration.body != nullptr)
    {
        forEachExpression(*declaration.body, [&](const Expression &expression) {
            if (expression.resolvedType.kind == TypeKind::Field)
            {
                node.usesFields = true;
            }
            if (const CallExpr *call = dynamic_cast<const CallExpr *>(&expression))
            {
                node.callees.insert(call->name);
            }
        });
    }

    return node;
}

// code generation gives every field variable, literal, copied parameter and call
// result a fixed block of memory. if a function could be running twice at once, the inner 
// call would overwrite the outer call's fields, so recursive functions
// (including mutual recursion, e.g. f calls g calls f) may not use fields at all
void SemanticAnalyser::checkRecursiveFieldUse(const Program &program)
{
    // one node per function or ruleset, only the first declaration of a name counts
    std::vector<CallGraphNode> nodes;
    std::unordered_map<std::string, std::size_t> nodeIndex;

    for (const std::unique_ptr<Declaration> &declaration : program.declarations)
    {
        CallGraphNode node;

        if (const FunctionDecl *function = dynamic_cast<const FunctionDecl *>(declaration.get()))
        {
            node = describeCallable(*function);
        }
        else if (const RulesetDecl *ruleset = dynamic_cast<const RulesetDecl *>(declaration.get()))
        {
            node = describeCallable(*ruleset);
        }
        else
        {
            continue;
        }

        if (nodeIndex.count(node.name) == 0)
        {
            nodeIndex[node.name] = nodes.size();
            nodes.push_back(std::move(node));
        }
    }

    for (const CallGraphNode &node : nodes)
    {
        if (!node.usesFields)
        {
            continue;
        }

        // recursive if following calls from it leads back to it
        std::unordered_set<std::string> visited;
        std::vector<std::string> toVisit(node.callees.begin(), node.callees.end());
        bool recursive = false;

        while (!toVisit.empty() && !recursive)
        {
            std::string name = toVisit.back();
            toVisit.pop_back();

            if (name == node.name)
            {
                recursive = true;
            }
            else if (visited.insert(name).second && nodeIndex.count(name) != 0)
            {
                const CallGraphNode &callee = nodes[nodeIndex[name]];
                toVisit.insert(toVisit.end(), callee.callees.begin(), callee.callees.end());
            }
        }

        if (recursive)
        {
            errors_.push_back(
                "Recursive function cannot use fields: '" + node.name +
                "' (fields have fixed memory, so a recursive call would overwrite them)");
        }
    }
}

// built-in names, and names starting with "__" (used for compiler-generated
// WAT names such as $__print_i32), cannot be declared
bool SemanticAnalyser::checkDeclarationName(const std::string &name)
{
    if (isBuiltin(name))
    {
        errors_.push_back(
            "Cannot redeclare built-in: '" + name + "'");
        return false;
    }

    if (name.rfind("__", 0) == 0)
    {
        errors_.push_back(
            "Reserved name: '" + name + "' (names starting with '__' are used by the compiler)");
        return false;
    }

    return true;
}

bool SemanticAnalyser::isBuiltin(const std::string &name)
{
    return name == "print" || name == "length" || isReduction(name);
}

// reductions combine every element of a field into one uniform
bool SemanticAnalyser::isReduction(const std::string &name)
{
    return name == "sum" || name == "min_of" || name == "max_of" ||
           name == "any" || name == "all";
}

Type SemanticAnalyser::analyseBuiltinCall(const CallExpr &call)
{
    // length(field) -> integer: the length is part of the type, so this is a constant
    // reductions for rulsewts:
    //   sum, min_of, max_of: field of integers or floats -> the element type
    //   any, all:            field of bools -> bool
    if (call.name == "length" || isReduction(call.name))
    {
        if (call.arguments.size() != 1)
        {
            errors_.push_back(
                "Incorrect number of arguments for function: '" + call.name + "'");
            return Type{TypeKind::Error, nullptr};
        }

        Type argumentType = analyseExpression(*call.arguments[0]);
        if (argumentType.kind == TypeKind::Error)
        {
            return Type{TypeKind::Error, nullptr};
        }

        if (argumentType.kind != TypeKind::Field || argumentType.elementType == nullptr)
        {
            errors_.push_back("'" + call.name + "' requires a field argument");
            return Type{TypeKind::Error, nullptr};
        }

        if (call.name == "length")
        {
            return Type{TypeKind::Int, nullptr};
        }

        if (!inRuleset_)
        {
            errors_.push_back("Reductions are only allowed in rulesets: '" + call.name + "'");
            return Type{TypeKind::Error, nullptr};
        }

        TypeKind element = argumentType.elementType->kind;
        bool logical = call.name == "any" || call.name == "all";

        if (logical && element != TypeKind::Bool)
        {
            errors_.push_back("'" + call.name + "' requires a field of bools");
            return Type{TypeKind::Error, nullptr};
        }

        if (!logical && element != TypeKind::Int && element != TypeKind::Float)
        {
            errors_.push_back("'" + call.name + "' requires a field of integers or floats");
            return Type{TypeKind::Error, nullptr};
        }

        return Type{element, nullptr};
    }

    if (call.name == "print")
    {
        if (call.arguments.size() != 1)
        {
            errors_.push_back(
                "Incorrect number of arguments for function: 'print'");
            return Type{TypeKind::Error, nullptr};
        }

        Type argumentType = analyseExpression(*call.arguments[0]);

        // an error was already reported inside the argument
        if (argumentType.kind == TypeKind::Error)
        {
            return Type{TypeKind::Error, nullptr};
        }

        if (argumentType.kind == TypeKind::Void)
        {
            errors_.push_back(
                "print requires an integer, float, bool or field argument");
            return Type{TypeKind::Error, nullptr};
        }

        return Type{TypeKind::Void, nullptr};
    }

    errors_.push_back("Unsupported built-in: '" + call.name + "'");
    return Type{TypeKind::Error, nullptr};
}

// registers a function and its type information in the symbol table
void SemanticAnalyser::registerFunction(const FunctionDecl &function)
{
    if (!checkDeclarationName(function.name))
    {
        return;
    }

    // create a symbol representing this function
    // stores its kind, return type and an empty parameter list
    Symbol symbol{
        SymbolKind::Function,
        function.returnType,
        {}};

    // go through the function's parameters
    // store each parameter type so function calls can be checked later
    for (const Parameter &parameter : function.parameters)
    {
        symbol.parameterTypes.push_back(parameter.type);
    }

    // attempt to add the function to the global symbol table
    // declare returns false if the name already exists in this scope
    if (!symbols_.declare(function.name, symbol))
    {
        // record an error if another declaration uses the same name
        errors_.push_back(
            "Duplicate declaration: '" + function.name + "'");
    }
}

// registers a ruleset and its type information in the symbol table
void SemanticAnalyser::registerRuleset(const RulesetDecl &ruleset)
{
    if (!checkDeclarationName(ruleset.name))
    {
        return;
    }

    // create a symbol representing this ruleset
    Symbol symbol{
        SymbolKind::Ruleset,
        ruleset.returnType,
        {}};

    // store the types of all ruleset parameters
    for (const Parameter &parameter : ruleset.parameters)
    {
        symbol.parameterTypes.push_back(parameter.type);
    }

    // attempt to register the ruleset in the global symbol table
    if (!symbols_.declare(ruleset.name, symbol))
    {
        // record an error if the name is already declared
        errors_.push_back(
            "Duplicate declaration: '" + ruleset.name + "'");
    }
}

// checks a function's parameters and body
// analyses a function body
void SemanticAnalyser::analyseFunction(const FunctionDecl &function)
{
    // remember the return type expected by this function
    currentReturnType_ = function.returnType;
    inRuleset_ = false;

    // an invalid return type is reported once
    if (!checkFieldElementType(function.returnType, "return type of '" + function.name + "'"))
    {
        currentReturnType_ = Type{TypeKind::Error, nullptr};
    }

    // create a scope for the function's parameters and variables
    symbols_.enterScope();

    // add each parameter to the function scope
    for (const Parameter &parameter : function.parameters)
    {
        checkFieldElementType(parameter.type, "parameter '" + parameter.name + "'");

        Symbol symbol{
            SymbolKind::Variable,
            parameter.type,
            {}};

        if (!symbols_.declare(parameter.name, symbol))
        {
            errors_.push_back(
                "Duplicate parameter: '" + parameter.name + "'");
        }
    }

    // analyse the function's statements
    if (function.body != nullptr)
    {
        analyseBlock(*function.body, false);

        // non-void functions must return on every path through the body
        if (function.returnType.kind != TypeKind::Void &&
            !alwaysReturns(*function.body))
        {
            errors_.push_back(
                "Missing return statement in function: '" +
                function.name + "'");
        }
    }
    else
    {
        // a function without a body cannot return a value
        if (function.returnType.kind != TypeKind::Void)
        {
            errors_.push_back(
                "Missing return statement in function: '" +
                function.name + "'");
        }
    }

    // remove the function scope
    symbols_.exitScope();
}

// checks a ruleset's parameters and statements
void SemanticAnalyser::analyseRuleset(const RulesetDecl &ruleset)
{
    // remember the expected return type
    currentReturnType_ = ruleset.returnType;
    inRuleset_ = true;

    if (!checkFieldElementType(ruleset.returnType, "return type of '" + ruleset.name + "'"))
    {
        currentReturnType_ = Type{TypeKind::Error, nullptr};
    }

    // create a scope for the ruleset parameters
    symbols_.enterScope();

    // add each parameter to the current scope
    for (const Parameter &parameter : ruleset.parameters)
    {
        checkFieldElementType(parameter.type, "parameter '" + parameter.name + "'");

        Symbol symbol{
            SymbolKind::Variable,
            parameter.type,
            {}};

        // report parameters with duplicate names
        if (!symbols_.declare(parameter.name, symbol))
        {
            errors_.push_back(
                "Duplicate parameter: '" + parameter.name + "'");
        }
    }

    // check all statements inside the ruleset
    if (ruleset.body != nullptr)
    {
        analyseBlock(*ruleset.body, false);
    }

    // a ruleset always produces a value, so it must end in a return
    if (ruleset.returnType.kind != TypeKind::Void &&
        (ruleset.body == nullptr || !alwaysReturns(*ruleset.body)))
    {
        errors_.push_back(
            "Missing return statement in ruleset: '" + ruleset.name + "'");
    }

    // remove the ruleset parameter scope
    symbols_.exitScope();
}

// checks all statements inside a block
// analyses the statements inside a block
void SemanticAnalyser::analyseBlock(
    const BlockStatement &block,
    bool createScope)
{
    // create a new scope for nested blocks
    if (createScope)
    {
        symbols_.enterScope();
    }

    // analyse every statement in the block
    for (const std::unique_ptr<Statement> &statement : block.statements)
    {
        analyseStatement(*statement);
    }

    // remove the scope if this block created one
    if (createScope)
    {
        symbols_.exitScope();
    }
}

// checks an individual statement inside a function
void SemanticAnalyser::analyseStatement(const Statement &statement)
{
    // 1. CHECK VARIABLE DECLARATIONS
    const LetStatement *variable =
        dynamic_cast<const LetStatement *>(&statement);

    if (variable != nullptr)
    {
        // start with the declared type
        Type variableType = variable->type;

        // an invalid declared type is reported once, without a type mismatch on top
        if (!checkFieldElementType(variableType, "variable '" + variable->name + "'"))
        {
            variableType = Type{TypeKind::Error, nullptr};
        }

        if (variable->initializer != nullptr)
        {
            // determine the initial value's type
            Type valueType =
                analyseExpression(*variable->initializer);

            // only infer a type if the expression is valid
            if (variableType.kind == TypeKind::Void)
            {
                if (valueType.kind == TypeKind::Error ||
                    valueType.kind == TypeKind::Void)
                {
                    errors_.push_back(
                        "Cannot infer type for variable: '" +
                        variable->name + "'");

                    variableType = Type{TypeKind::Error, nullptr};
                }
                else
                {
                    variableType = valueType;
                }
            }
            // check explicitly declared variable types
            else if (valueType.kind != TypeKind::Error &&
                     variableType.kind != TypeKind::Error &&
                     !sameType(valueType, variableType))
            {
                errors_.push_back(
                    "Type mismatch for variable: '" +
                    variable->name + "'");
            }
        }
        else if (variableType.kind == TypeKind::Void)
        {
            errors_.push_back(
                "Cannot infer type for variable: '" +
                variable->name + "'");

            variableType = Type{TypeKind::Error, nullptr};
        }

        // record the final type on the AST so codegen can read it
        variable->resolvedType = variableType;

        // store the declared or inferred type
        Symbol symbol{
            SymbolKind::Variable,
            variableType,
            {}};

        if (!symbols_.declare(variable->name, symbol))
        {
            errors_.push_back(
                "Duplicate variable: '" + variable->name + "'");
        }

        return;
    }
    // 2. CHECK RETURN STATEMENTS
    const ReturnStatement *returnStatement =
        dynamic_cast<const ReturnStatement *>(&statement);

    if (returnStatement != nullptr)
    {
        // check if the return statement contains a value
        if (returnStatement->value != nullptr)
        {
            // determine the type of the returned value
            Type returnType =
                analyseExpression(*returnStatement->value);

            // void functions cannot return a value
            if (currentReturnType_.kind == TypeKind::Void)
            {
                errors_.push_back(
                    "Void function cannot return a value");
            }
            // check if the returned type matches the function type
            else if (returnType.kind != TypeKind::Error &&
                     currentReturnType_.kind != TypeKind::Error &&
                     !sameType(returnType, currentReturnType_))
            {
                errors_.push_back("Return type mismatch");
            }
        }
        else
        {
            // non-void functions must return a value
            if (currentReturnType_.kind != TypeKind::Void)
            {
                errors_.push_back(
                    "Non-void function must return a value");
            }
        }

        return;
    }

    // 3. CHECK EXPRESSION STATEMENTS
    const ExpressionStatement *expressionStatement =
        dynamic_cast<const ExpressionStatement *>(&statement);

    if (expressionStatement != nullptr)
    {
        // check the expression for semantic errors
        if (expressionStatement->expression != nullptr)
        {
            analyseExpression(*expressionStatement->expression);
        }

        return;
    }

    // 4. CHECK NESTED BLOCK STATEMENTS
    const BlockStatement *block =
        dynamic_cast<const BlockStatement *>(&statement);

    if (block != nullptr)
    {
        // analyse all statements inside this nested block
        analyseBlock(*block);

        return;
    }

    // 5. CHECK ASSIGNMENTS
    const AssignmentStatement *assignment =
        dynamic_cast<const AssignmentStatement *>(&statement);

    if (assignment != nullptr)
    {
        // analyse expression to check for errors
        Type valueType = analyseExpression(*assignment->value);

        // the target must already exist
        const Symbol *symbol = symbols_.lookup(assignment->name);

        if (symbol == nullptr)
        {
            errors_.push_back(
                "Undefined variable: '" + assignment->name + "'");

            return;
        }

        // functions and rulesets cannot be assigned to
        if (symbol->kind != SymbolKind::Variable)
        {
            errors_.push_back(
                "Not a variable: '" + assignment->name + "'");

            return;
        }

        // the value must match the variable's type
        if (valueType.kind != TypeKind::Error &&
            symbol->type.kind != TypeKind::Error &&
            !sameType(valueType, symbol->type))
        {
            errors_.push_back(
                "Type mismatch in assignment to: '" +
                assignment->name + "'");
        }

        return;
    }

    // 6. CHECK IF STATEMENTS
    const IfStatement *ifStatement =
        dynamic_cast<const IfStatement *>(&statement);

    if (ifStatement != nullptr)
    {
        analyseCondition(*ifStatement->condition, "if");

        // each branch is a block, so it gets its own scope
        analyseBlock(*ifStatement->thenBlock);

        // the else branch is a block or another if (else if)
        if (ifStatement->elseBranch != nullptr)
        {
            analyseStatement(*ifStatement->elseBranch);
        }

        return;
    }

    // 7. CHECK WHILE STATEMENTS
    const WhileStatement *whileStatement =
        dynamic_cast<const WhileStatement *>(&statement);

    if (whileStatement != nullptr)
    {
        analyseCondition(*whileStatement->condition, "while");
        analyseBlock(*whileStatement->body);

        return;
    }
}

// checks that an if / while condition is a boolean
void SemanticAnalyser::analyseCondition(
    const Expression &condition,
    const std::string &statementName)
{
    Type conditionType = analyseExpression(condition);

    // an error was already reported inside the condition
    if (conditionType.kind == TypeKind::Error)
    {
        return;
    }

    if (conditionType.kind != TypeKind::Bool)
    {
        errors_.push_back(
            "Condition of '" + statementName + "' must be a boolean");
    }
}

// checks an expression, stores its type on the AST node and returns it
// every expression, including sub-expressions, passes through here
Type SemanticAnalyser::analyseExpression(const Expression &expression)
{
    Type type = inferExpressionType(expression);
    expression.resolvedType = type;
    return type;
}

// checks an expression and returns its data type
Type SemanticAnalyser::inferExpressionType(const Expression &expression)
{
    // check if the expression is a literal value
    // examples: 10, 5.5, true
    const LiteralExpr *literal =
        dynamic_cast<const LiteralExpr *>(&expression);

    if (literal != nullptr)
    {
        // literals already store their type in the AST
        return literal->type;
    }

    // check if the expression is a field literal
    // examples: [1.0, 2.0], [x, x + 1, 0]
    const FieldLiteralExpr *fieldLiteral =
        dynamic_cast<const FieldLiteralExpr *>(&expression);

    if (fieldLiteral != nullptr)
    {
        return analyseFieldLiteral(*fieldLiteral);
    }

    // check if the expression is a variable reference
    // examples: x, y, result
    const VariableExpr *variable =
        dynamic_cast<const VariableExpr *>(&expression);

    if (variable != nullptr)
    {
        // search the symbol table for this variable
        const Symbol *symbol = symbols_.lookup(variable->name);

        // if the name does not exist, report an error
        if (symbol == nullptr)
        {
            errors_.push_back(
                "Undefined variable: '" + variable->name + "'");

            return Type{TypeKind::Error, nullptr};
        }

        // make sure the name represents a variable
        // rather than a function or ruleset
        if (symbol->kind != SymbolKind::Variable)
        {
            errors_.push_back(
                "Not a variable: '" + variable->name + "'");

            return Type{TypeKind::Error, nullptr};
        }

        // return the variable's type from the symbol table
        return symbol->type;
    }

    // check if the expression is a binary operation
    // examples: a + b, x * 5, x > 10
    const BinaryExpr *binary =
        dynamic_cast<const BinaryExpr *>(&expression);

    if (binary != nullptr)
    {
        // check the left side of the expression
        Type leftType = analyseExpression(*binary->left);

        // check the right side of the expression
        Type rightType = analyseExpression(*binary->right);

        // stop if either side already has an error
        if (leftType.kind == TypeKind::Error ||
            rightType.kind == TypeKind::Error)
        {
            return Type{TypeKind::Error, nullptr};
        }

        // element-wise operations, e.g. a + b, a * 2.0, a < b
        if (leftType.kind == TypeKind::Field ||
            rightType.kind == TypeKind::Field)
        {
            return analyseFieldBinary(binary->op, leftType, rightType);
        }

        // both sides must have matching types
        if (!sameType(leftType, rightType))
        {
            errors_.push_back("Binary expression has mismatched types");
            return Type{TypeKind::Error, nullptr};
        }

        // check arithmetic operators: +, -, *, /
        if (binary->op == BinaryOperator::Add ||
            binary->op == BinaryOperator::Subtract ||
            binary->op == BinaryOperator::Multiply ||
            binary->op == BinaryOperator::Divide)
        {
            // arithmetic is currently allowed for integers and floats
            if (leftType.kind != TypeKind::Int &&
                leftType.kind != TypeKind::Float)
            {
                errors_.push_back(
                    "Arithmetic requires numeric types");

                return Type{TypeKind::Error, nullptr};
            }

            // arithmetic returns the same type as its operands
            return leftType;
        }

        // check equality operators: == and !=
        if (binary->op == BinaryOperator::Equal ||
            binary->op == BinaryOperator::NotEqual)
        {
            // equality returns a boolean
            if (leftType.kind == TypeKind::Int ||
                leftType.kind == TypeKind::Float ||
                leftType.kind == TypeKind::Bool)
            {
                return Type{TypeKind::Bool, nullptr};
            }

            errors_.push_back("Unsupported equality operand type");
            return Type{TypeKind::Error, nullptr};
        }

        // check comparison operators: <, >, <=, >=
        if (binary->op == BinaryOperator::LessThan ||
            binary->op == BinaryOperator::GreaterThan ||
            binary->op == BinaryOperator::LessThanEqual ||
            binary->op == BinaryOperator::GreaterThanEqual)
        {
            // comparisons currently require integers or floats
            if (leftType.kind != TypeKind::Int &&
                leftType.kind != TypeKind::Float)
            {
                errors_.push_back(
                    "Comparison requires numeric types");

                return Type{TypeKind::Error, nullptr};
            }

            // comparisons return true or false
            return Type{TypeKind::Bool, nullptr};
        }

        // report any binary operator we have not implemented
        errors_.push_back("Unsupported binary operator");

        return Type{TypeKind::Error, nullptr};
    }

    // check if the expression is a unary operation
    // examples: -10, -x
    const UnaryExpr *unary =
        dynamic_cast<const UnaryExpr *>(&expression);

    if (unary != nullptr)
    {
        // check the expression being operated on
        Type operandType = analyseExpression(*unary->operand);

        // stop if the operand already has an error
        if (operandType.kind == TypeKind::Error)
        {
            return Type{TypeKind::Error, nullptr};
        }

        // check the negation operator (-)
        if (unary->op == UnaryOperator::Negate)
        {
            // -a negates every element
            if (operandType.kind == TypeKind::Field)
            {
                if (!checkInRuleset())
                {
                    return Type{TypeKind::Error, nullptr};
                }

                TypeKind element = operandType.elementType->kind;
                if (element != TypeKind::Int && element != TypeKind::Float)
                {
                    errors_.push_back(
                        "Negation requires a numeric type");

                    return Type{TypeKind::Error, nullptr};
                }

                return operandType;
            }

            // negation only works with integers and floats
            if (operandType.kind != TypeKind::Int &&
                operandType.kind != TypeKind::Float)
            {
                errors_.push_back(
                    "Negation requires a numeric type");

                return Type{TypeKind::Error, nullptr};
            }

            // negation returns the same type as the operand
            return operandType;
        }

        // report an unsupported unary operator
        errors_.push_back("Unsupported unary operator");

        return Type{TypeKind::Error, nullptr};
    }

    // check if the expression is a function call
    // examples: add(5, 10), calculate(x)
    const CallExpr *call =
        dynamic_cast<const CallExpr *>(&expression);

    if (call != nullptr)
    {
        // built-ins are not in the symbol table and cannot be redeclared
        if (isBuiltin(call->name))
        {
            return analyseBuiltinCall(*call);
        }

        // find the function in the symbol table
        const Symbol *symbol = symbols_.lookup(call->name);

        // check if the function exists
        if (symbol == nullptr)
        {
            errors_.push_back(
                "Undefined function: '" + call->name + "'");

            return Type{TypeKind::Error, nullptr};
        }

        // make sure the name belongs to a function or ruleset (both are called the same way)
        if (symbol->kind != SymbolKind::Function &&
            symbol->kind != SymbolKind::Ruleset)
        {
            errors_.push_back(
                "Not a function: '" + call->name + "'");

            return Type{TypeKind::Error, nullptr};
        }

        // check the number of arguments
        if (call->arguments.size() != symbol->parameterTypes.size())
        {
            errors_.push_back(
                "Incorrect number of arguments for function: '" +
                call->name + "'");

            return Type{TypeKind::Error, nullptr};
        }

        // track whether any argument contains an error
        bool hasError = false;

        // check each argument against its expected parameter type
        for (std::size_t i = 0; i < call->arguments.size(); i++)
        {
            // determine the type of the current argument
            Type argumentType =
                analyseExpression(*call->arguments[i]);

            // get the type expected by the function
            Type parameterType = symbol->parameterTypes[i];

            // check if the argument already contains an error
            if (argumentType.kind == TypeKind::Error)
            {
                hasError = true;
            }
            // check if the argument type matches the parameter type
            else if (!sameType(argumentType, parameterType))
            {
                errors_.push_back(
                    "Argument type mismatch in function: '" +
                    call->name + "'");

                hasError = true;
            }
        }

        // return an error if any argument was invalid
        if (hasError)
        {
            return Type{TypeKind::Error, nullptr};
        }

        // return the function's declared return type
        return symbol->type;
    }

    // report expression types that are not implemented yet
    errors_.push_back("Unsupported expression");

    return Type{TypeKind::Error, nullptr};
}

// field arithmetic, comparison and negation are only allowed inside rulesets
// normal functions can still create, copy, pass and print fields
bool SemanticAnalyser::checkInRuleset()
{
    if (!inRuleset_)
    {
        errors_.push_back(
            "Field operations are only allowed in rulesets");
        return false;
    }
    return true;
}

// a binary operation where at least one side is a field, applied to each element:
//   field <op> field     both fields must have the same type (element type and length)
//   field <op> uniform   the uniform is used for every element (broadcast), so it must
//   uniform <op> field   have the field's element type, e.g. field<f32, 4> * f32
// arithmetic gives a field of the same type, comparisons give field<bool, same length>
Type SemanticAnalyser::analyseFieldBinary(
    BinaryOperator op,
    const Type &left,
    const Type &right)
{
    if (!checkInRuleset())
    {
        return Type{TypeKind::Error, nullptr};
    }

    const Type &field = left.kind == TypeKind::Field ? left : right;
    const Type &other = left.kind == TypeKind::Field ? right : left;
    const Type &element = *field.elementType;

    bool compatible = other.kind == TypeKind::Field
                          ? sameType(left, right)
                          : sameType(other, element);

    if (!compatible)
    {
        errors_.push_back("Binary expression has mismatched types");
        return Type{TypeKind::Error, nullptr};
    }

    Type boolField{TypeKind::Field, std::make_shared<Type>(Type{TypeKind::Bool, nullptr})};
    boolField.length = field.length;

    switch (op)
    {
    case BinaryOperator::Add:
    case BinaryOperator::Subtract:
    case BinaryOperator::Multiply:
        if (element.kind != TypeKind::Int && element.kind != TypeKind::Float)
        {
            errors_.push_back("Arithmetic requires numeric types");
            return Type{TypeKind::Error, nullptr};
        }
        return field;

    // SIMD has no integer division
    case BinaryOperator::Divide:
        if (element.kind != TypeKind::Float)
        {
            errors_.push_back("Division of fields requires float elements");
            return Type{TypeKind::Error, nullptr};
        }
        return field;

    case BinaryOperator::Equal:
    case BinaryOperator::NotEqual:
        return boolField;

    case BinaryOperator::LessThan:
    case BinaryOperator::GreaterThan:
    case BinaryOperator::LessThanEqual:
    case BinaryOperator::GreaterThanEqual:
        if (element.kind != TypeKind::Int && element.kind != TypeKind::Float)
        {
            errors_.push_back("Comparison requires numeric types");
            return Type{TypeKind::Error, nullptr};
        }
        return boolField;
    }

    errors_.push_back("Unsupported binary operator");
    return Type{TypeKind::Error, nullptr};
}

// field<T, N> written in a declaration may only hold i32, f32 or bool
// (fields of fields, or of void, cannot be laid out as SIMD lanes)
bool SemanticAnalyser::checkFieldElementType(const Type &type, const std::string &context)
{
    if (type.kind != TypeKind::Field)
    {
        return true;
    }

    TypeKind element = type.elementType == nullptr ? TypeKind::Error : type.elementType->kind;

    if (element != TypeKind::Int && element != TypeKind::Float && element != TypeKind::Bool)
    {
        errors_.push_back(
            "Field elements must be integer, float or bool (" + context + ")");
        return false;
    }

    return true;
}

// a field literal's type is field<element type, number of elements>
// every element must have the same type, and that type must be i32, f32 or bool
Type SemanticAnalyser::analyseFieldLiteral(const FieldLiteralExpr &literal)
{
    Type elementType{TypeKind::Error, nullptr};
    bool hasError = false;

    for (const std::unique_ptr<Expression> &element : literal.elements)
    {
        Type type = analyseExpression(*element);

        // an error was already reported inside this element
        if (type.kind == TypeKind::Error)
        {
            hasError = true;
            continue;
        }

        if (type.kind != TypeKind::Int &&
            type.kind != TypeKind::Float &&
            type.kind != TypeKind::Bool)
        {
            errors_.push_back(
                "Field elements must be integer, float or bool");
            hasError = true;
            continue;
        }

        // the first valid element decides the type the rest must match
        if (elementType.kind == TypeKind::Error)
        {
            elementType = type;
        }
        else if (!sameType(type, elementType))
        {
            errors_.push_back(
                "Field literal elements must all have the same type");
            hasError = true;
        }
    }

    if (hasError)
    {
        return Type{TypeKind::Error, nullptr};
    }

    Type fieldType{TypeKind::Field, std::make_shared<Type>(elementType)};
    fieldType.length = static_cast<std::uint32_t>(literal.elements.size());
    return fieldType;
}

// allows other parts of the compiler to read semantic errors
const std::vector<std::string> &SemanticAnalyser::getErrors() const
{
    return errors_;
}

// compares two types, including field element types
bool SemanticAnalyser::sameType(
    const Type &left,
    const Type &right) const
{
    // the main types must match
    if (left.kind != right.kind)
    {
        return false;
    }

    // ordinary types are equal if their kinds match
    if (left.kind != TypeKind::Field)
    {
        return true;
    }

    // fields must both have an element type
    if (left.elementType == nullptr ||
        right.elementType == nullptr)
    {
        return false;
    }

    // fields of different lengths are different types
    if (left.length != right.length)
    {
        return false;
    }

    // compare the types stored inside the fields
    return sameType(*left.elementType, *right.elementType);
}

// checks whether every path through a statement ends in a return
bool SemanticAnalyser::alwaysReturns(const Statement &statement) const
{
    if (dynamic_cast<const ReturnStatement *>(&statement) != nullptr)
    {
        return true;
    }

    // a block returns if any of its statements always returns
    // anything after that statement can never run
    const BlockStatement *block =
        dynamic_cast<const BlockStatement *>(&statement);

    if (block != nullptr)
    {
        return alwaysReturns(*block);
    }

    // an if returns only when it has an else and both branches always return
    const IfStatement *ifStatement =
        dynamic_cast<const IfStatement *>(&statement);

    if (ifStatement != nullptr)
    {
        return ifStatement->thenBlock != nullptr &&
               ifStatement->elseBranch != nullptr &&
               alwaysReturns(*ifStatement->thenBlock) &&
               alwaysReturns(*ifStatement->elseBranch);
    }

    return false;
}

// checks whether every path through a block ends in a return
bool SemanticAnalyser::alwaysReturns(const BlockStatement &block) const
{
    for (const std::unique_ptr<Statement> &statement : block.statements)
    {
        if (alwaysReturns(*statement))
        {
            return true;
        }
    }

    return false;
}
