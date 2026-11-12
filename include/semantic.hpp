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
    void analyseFunction(const FunctionDecl &function);
    void analyseBlock(const BlockStatement &block, bool createScope = true);
    void analyseStatement(const Statement &statement);
    void analyseRuleset(const RulesetDecl &ruleset);
    void analyseCondition(const Expression &condition, const std::string &statementName);
    Type analyseExpression(const Expression &expression);
    Type inferExpressionType(const Expression &expression);
    Type currentReturnType_{TypeKind::Void, nullptr};

    bool sameType(const Type &left, const Type &right) const;
    // true if every path through the statement ends in a return
    bool alwaysReturns(const Statement &statement) const;
    bool alwaysReturns(const BlockStatement &block) const;
};

#endif // SEMANTIC_H