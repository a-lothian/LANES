#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <string>
#include <vector>

#include "ast.hpp"
#include "symbol_table.hpp"

class SemanticAnalyser
{
public:
    bool analyse(const Program &program);

    const std::vector<std::string> &getErrors() const;

private:
    SymbolTable symbols_;
    std::vector<std::string> errors_;

    void registerFunction(const FunctionDecl &function);
    void registerRuleset(const RulesetDecl &ruleset);
};

#endif // SEMANTIC_H