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

    // return true if no semantic errors were found
    return errors_.empty();
}

// registers a function and its type information in the symbol table
void SemanticAnalyser::registerFunction(const FunctionDecl &function)
{
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

    // create a scope for the function's parameters and variables
    symbols_.enterScope();

    // add each parameter to the function scope
    for (const Parameter &parameter : function.parameters)
    {
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

    // create a scope for the ruleset parameters
    symbols_.enterScope();

    // add each parameter to the current scope
    for (const Parameter &parameter : ruleset.parameters)
    {
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
        // find the function in the symbol table
        const Symbol *symbol = symbols_.lookup(call->name);

        // check if the function exists
        if (symbol == nullptr)
        {
            errors_.push_back(
                "Undefined function: '" + call->name + "'");

            return Type{TypeKind::Error, nullptr};
        }

        // make sure the name belongs to a function
        if (symbol->kind != SymbolKind::Function)
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
