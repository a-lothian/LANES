#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"

// represents the different kinds of symbols
enum class SymbolKind
{
    Variable,
    Parameter,
    Function,
    Ruleset
};

// stores information about a symbol
struct Symbol
{
    SymbolKind kind;
    Type type;
    std::vector<Type> parameterTypes;
    bool readOnly = false;  // a for loop variable which cannot be assigned
};

// manages symbols and their scopes
class SymbolTable
{
public:
    // creates the global scope
    SymbolTable();

    // creates and removes scopes
    void enterScope();
    void exitScope();

    // adds a symbol to the current scope
    bool declare(const std::string &name, const Symbol &symbol);

    // searches all accessible scopes
    const Symbol *lookup(const std::string &name) const;

    // searches only the current scope
    const Symbol *lookupCurrentScope(const std::string &name) const;

private:
    // each map represents one scope
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

#endif // SYMBOL_TABLE_H