#include "semantic.hpp"

// goes through the program and checks its declarations
bool SemanticAnalyser::analyse(const Program &program)
{
    // reset the symbol table and errors before checking a program
    // this prevents declarations and errors from previous runs carrying over
    symbols_ = SymbolTable();
    errors_.clear();

    // go through every declaration stored in the program AST
    // each declaration is stored as a pointer to the base Declaration class
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

    // go through declarations again to check function bodies
    for (const std::unique_ptr<Declaration> &declaration : program.declarations)
    {
        const FunctionDecl *function =
            dynamic_cast<const FunctionDecl *>(declaration.get());

        if (function != nullptr)
        {
            analyseFunction(*function);
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
void SemanticAnalyser::analyseFunction(const FunctionDecl &function)
{
    // remember what type this function should return
    currentReturnType_ = function.returnType;

    // create a new scope for this function
    symbols_.enterScope();

    // go through each parameter in the function
    for (const Parameter &parameter : function.parameters)
    {
        // create a symbol representing the parameter
        Symbol symbol{
            SymbolKind::Variable,
            parameter.type,
            {}};

        // add the parameter to the function's scope
        // if the name already exists, report an error
        if (!symbols_.declare(parameter.name, symbol))
        {
            errors_.push_back(
                "Duplicate parameter: '" + parameter.name + "'");
        }
    }

    // check the statements inside the function
    analyseBlock(*function.body);

    // remove the function's scope when finished
    symbols_.exitScope();
}

// goes through every statement inside a block
void SemanticAnalyser::analyseBlock(const BlockStatement &block)
{
    // loop through each statement stored in the block
    for (const std::unique_ptr<Statement> &statement : block.statements)
    {
        // check the current statement
        analyseStatement(*statement);
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
        // check the initial value if one was provided
        if (variable->initializer != nullptr)
        {
            Type valueType =
                analyseExpression(*variable->initializer);

            // check if the value matches the declared type
            if (valueType.kind != TypeKind::Void &&
                valueType.kind != variable->type.kind)
            {
                errors_.push_back(
                    "Type mismatch for variable: '" + variable->name + "'");
            }
        }

        // create a symbol for the variable
        Symbol symbol{
            SymbolKind::Variable,
            variable->type,
            {}};

        // add the variable to the current scope
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
            else if (returnType.kind != TypeKind::Void &&
                     returnType.kind != currentReturnType_.kind)
            {
                errors_.push_back(
                    "Return type mismatch");
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
}

// checks an expression and returns its data type
Type SemanticAnalyser::analyseExpression(const Expression &expression)
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

            return Type{TypeKind::Void};
        }

        // make sure the name represents a variable
        // rather than a function or ruleset
        if (symbol->kind != SymbolKind::Variable)
        {
            errors_.push_back(
                "Not a variable: '" + variable->name + "'");

            return Type{TypeKind::Void};
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
        if (leftType.kind == TypeKind::Void ||
            rightType.kind == TypeKind::Void)
        {
            return Type{TypeKind::Void, nullptr};
        }

        // both sides must have matching types
        if (leftType.kind != rightType.kind)
        {
            errors_.push_back(
                "Binary expression has mismatched types");

            return Type{TypeKind::Void, nullptr};
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

                return Type{TypeKind::Void, nullptr};
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
            return Type{TypeKind::Void, nullptr};
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

                return Type{TypeKind::Void, nullptr};
            }

            // comparisons return true or false
            return Type{TypeKind::Bool, nullptr};
        }

        // report any binary operator we have not implemented
        errors_.push_back("Unsupported binary operator");

        return Type{TypeKind::Void, nullptr};
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
        if (operandType.kind == TypeKind::Void)
        {
            return Type{TypeKind::Void, nullptr};
        }

        // check the negation operator (-)
        if (unary->op == UnaryOperator::Negate)
        {
            // negation only works with integers and floats for now
            if (operandType.kind != TypeKind::Int &&
                operandType.kind != TypeKind::Float)
            {
                errors_.push_back(
                    "Negation requires a numeric type");

                return Type{TypeKind::Void, nullptr};
            }

            // negation returns the same type as the operand
            return operandType;
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

                return Type{TypeKind::Void, nullptr};
            }

            // make sure the name belongs to a function
            if (symbol->kind != SymbolKind::Function)
            {
                errors_.push_back(
                    "Not a function: '" + call->name + "'");

                return Type{TypeKind::Void, nullptr};
            }

            // check the number of arguments
            if (call->arguments.size() != symbol->parameterTypes.size())
            {
                errors_.push_back(
                    "Incorrect number of arguments for function: '" +
                    call->name + "'");

                return Type{TypeKind::Void, nullptr};
            }

            // check each argument against its expected parameter type
            for (std::size_t i = 0; i < call->arguments.size(); i++)
            {
                // determine the type of the current argument
                Type argumentType =
                    analyseExpression(*call->arguments[i]);

                // get the type expected by the function
                Type parameterType = symbol->parameterTypes[i];

                // check if the argument type matches
                if (argumentType.kind != TypeKind::Void &&
                    argumentType.kind != parameterType.kind)
                {
                    errors_.push_back(
                        "Argument type mismatch in function: '" +
                        call->name + "'");
                }
            }

            // return the function's declared return type
            return symbol->type;
        }

        // report an operator we have not implemented
        errors_.push_back("Unsupported unary operator");

        return Type{TypeKind::Void, nullptr};
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

            return Type{TypeKind::Void, nullptr};
        }

        // make sure the name belongs to a function
        if (symbol->kind != SymbolKind::Function)
        {
            errors_.push_back(
                "Not a function: '" + call->name + "'");

            return Type{TypeKind::Void, nullptr};
        }

        // check the number of arguments
        if (call->arguments.size() != symbol->parameterTypes.size())
        {
            errors_.push_back(
                "Incorrect number of arguments for function: '" +
                call->name + "'");

            return Type{TypeKind::Void, nullptr};
        }

        // check each argument against its expected parameter type
        for (std::size_t i = 0; i < call->arguments.size(); i++)
        {
            // determine the type of the current argument
            Type argumentType =
                analyseExpression(*call->arguments[i]);

            // get the type expected by the function
            Type parameterType = symbol->parameterTypes[i];

            // check if the argument type matches
            if (argumentType.kind != TypeKind::Void &&
                argumentType.kind != parameterType.kind)
            {
                errors_.push_back(
                    "Argument type mismatch in function: '" +
                    call->name + "'");
            }
        }

        // return the function's declared return type
        return symbol->type;
    }

    // report expression types that are not implemented yet
    errors_.push_back("Unsupported expression");

    return Type{TypeKind::Void};
}

// allows other parts of the compiler to read semantic errors
const std::vector<std::string> &SemanticAnalyser::getErrors() const
{
    return errors_;
}