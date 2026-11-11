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

// allows other parts of the compiler to read semantic errors
const std::vector<std::string> &SemanticAnalyser::getErrors() const
{
    return errors_;
}