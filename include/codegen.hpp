#ifndef LANES_CODEGEN_HPP
#define LANES_CODEGEN_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.hpp"
#include "memory_layout.hpp"
#include "wat_emitter.hpp"

class CodeGenerator
{
public:
    // returns true if WAT was generated without errors
 bool generate(const Program& program);

 // the generated module
 const std::string& getWat() const;

 const std::vector<std::string>& getErrors() const;

private:
    WatModule module_;
    std::string wat_;
    std::vector<std::string> errors_;

    // state for the function currently being generated
    WatFunction *function_ = nullptr;

    // give each variables unique names across scopes
    std::vector<std::unordered_map<std::string, std::string>> scopes_;
    std::unordered_set<std::string> usedLocalNames_;
    int labelCounter_ = 0;

    // fixed addresses for fields made at compiletime
    // the module only gets a memory section if something was reserved
    MemoryLayout memory_;

    void generateFunction(const FunctionDecl &function);

    void generateBlock(const BlockStatement &block, bool createScope = true);
    void generateStatement(const Statement &statement);
    void generateLet(const LetStatement &statement);
    void generateIf(const IfStatement &statement);
    void generateWhile(const WhileStatement &statement);

    void generateExpression(const Expression &expression);
    void generateLiteral(const LiteralExpr &literal);
    void generateFieldLiteral(const FieldLiteralExpr &literal);
    void generateCall(const CallExpr &call);

    // fields use copy value semantics
    bool reserveField(const Type &type, std::uint32_t &address, const std::string &context);
    void writeFieldLiteral(const FieldLiteralExpr &literal, std::uint32_t address);
    void copyField(const Type &type);
    void generateFieldSource(const Expression &expression);
    void generateFieldLet(const LetStatement &statement);
    void generateFieldAssignment(const AssignmentStatement &assignment);
    void copyAssignedFieldParameters(const FunctionDecl &function);

    // every field element (i32, f32 or bool) takes 4 bytes
    static constexpr std::uint32_t FIELD_ELEMENT_BYTES = 4;
    void generateUnary(const UnaryExpr &unary);
    void generateBinary(const BinaryExpr &binary);

    // scope and naming helpers
    void enterScope();
    void exitScope();
    std::string declareLocal(const std::string &name, const Type &type);
    std::string declareParameter(const std::string &name);
    std::string lookupLocal(const std::string &name);

    // maps a LANES type to a WAT type if unsupported
    bool toWatType(const Type &type, WatType &result, const std::string &context);

    void addError(const std::string &message);
    void reset();
};

#endif // LANES_CODEGEN_HPP
