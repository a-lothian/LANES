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
};

#endif // SYMBOL_TABLE_H