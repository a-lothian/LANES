#include "symbol_table.hpp"

// creates the global scope
SymbolTable::SymbolTable()
{
    enterScope();
}

// creates a new scope
void SymbolTable::enterScope()
{
    scopes_.emplace_back();
}

// removes the current scope
void SymbolTable::exitScope()
{
    if (scopes_.size() > 1)
    {
        scopes_.pop_back();
    }
}

// adds a symbol to the current scope
bool SymbolTable::declare(const std::string &name, const Symbol &symbol)
{
    std::unordered_map<std::string, Symbol> &currentScope = scopes_.back();

    return currentScope.emplace(name, symbol).second;
}

// searches from the current scope to the global scope
const Symbol *SymbolTable::lookup(const std::string &name) const
{
    // start from the last scope and work backwards
    for (size_t i = scopes_.size(); i > 0; i--)
    {
        const std::unordered_map<std::string, Symbol> &currentScope = scopes_[i - 1];

        std::unordered_map<std::string, Symbol>::const_iterator found =
            currentScope.find(name);

        if (found != currentScope.end())
        {
            return &found->second;
        }
    }

    return nullptr;
}

// searches only the current scope
const Symbol *SymbolTable::lookupCurrentScope(const std::string &name) const
{
    const std::unordered_map<std::string, Symbol> &currentScope = scopes_.back();

    std::unordered_map<std::string, Symbol>::const_iterator found =
        currentScope.find(name);

    if (found != currentScope.end())
    {
        return &found->second;
    }

    return nullptr;
}