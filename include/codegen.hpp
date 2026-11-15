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
    // true if the host needs to read memory (printing a field), even if nothing was reserved
    bool hostReadsMemory_ = false;

    // host functions imported from "env", added on first use
    std::unordered_set<std::string> importedFunctions_;
    void requireImport(const std::string &name, const std::vector<WatType> &params);

    void generateCallable(const std::string &name, const std::vector<Parameter> &parameters,
                          const Type &returnType, const BlockStatement *body, bool isRuleset);

    // SIMD lowering of a ruleset that uses element-wise field operations
    // pass 1 (before the loop): scalar statements, uniforms splatted into v128 locals,
    // field literals and calls evaluated to addresses
    // pass 2 (the loop): one iteration per 16 byte chunk, field values are v128 locals
    void generateVectorisedRuleset(const std::string &name, const std::vector<Parameter> &parameters,
                                   const Type &returnType, const BlockStatement &body);
    bool prepareVectorStatement(const Statement &statement, bool isLast);
    bool prepareTailStatement(const Statement &statement, bool isLast);
    void prepareVectorExpression(const Expression &expression);
    void generateVectorExpression(const Expression &expression);
    void generateVectorStatement(const Statement &statement);
    void checkVectorLength(std::uint32_t length);
    std::string parameterChunk(const std::string &parameter);
    void vectorError(const std::string &reason);

    // sum, min_of, max_of, any or all: a v128 accumulator updated once per chunk,
    // then folded into one scalar after the loop
    struct Reduction
    {
        const CallExpr *call = nullptr;
        TypeKind element = TypeKind::Error;  // element type of the argument
        std::string accumulator;             // v128 local
        std::string result;                  // i32 or f32 local, set after the loop
    };
    void prepareReduction(const CallExpr &call);
    void pushReductionIdentity(const Reduction &reduction);
    void combineReduction(const Reduction &reduction);
    void finishReduction(const Reduction &reduction);

    struct VectorState
    {
        bool active = false;
        std::string ruleset;
        std::uint32_t length = 0;  // every field in the loop has this length
        std::uint32_t result = 0;  // address of the returned field's block
        std::string offset;        // i32 local; byte offset of the current chunk
        bool lengthMismatchReported = false;

        std::unordered_map<const Expression *, std::string> splats;     // uniform -> v128 local
        std::unordered_map<const Expression *, std::string> addresses;  // literal or call -> i32 address local
        std::unordered_map<const Expression *, std::string> variables;  // field variable -> v128 local
        std::unordered_map<const Statement *, std::string> targets;     // let or assignment -> v128 local
        std::unordered_set<std::string> vectorLocals;                   // field lets
        std::unordered_set<std::string> assignedParameters;             // field parameters assigned in the body
        std::vector<std::pair<std::string, std::string>> parameterChunks;  // parameter -> v128 local, loaded each chunk

        std::vector<Reduction> reductions;
        std::unordered_map<const CallExpr *, std::string> reductionResults;  // reduction call -> result local
        std::unordered_set<std::string> tailNames;  // variables set after the first reduction
    };
    VectorState vector_;

    void generateBlock(const BlockStatement &block, bool createScope = true);
    void generateStatement(const Statement &statement);
    void generateLet(const LetStatement &statement);
    void generateIf(const IfStatement &statement);
    void generateWhile(const WhileStatement &statement);

    void generateExpression(const Expression &expression);
    void generateLiteral(const LiteralExpr &literal);
    void generateFieldLiteral(const FieldLiteralExpr &literal);
    void generateCall(const CallExpr &call);
    void generatePrint(const CallExpr &call);

    // fields use copy value semantics
    bool reserveField(const Type &type, std::uint32_t &address, const std::string &context);
    void writeFieldLiteral(const FieldLiteralExpr &literal, std::uint32_t address);
    void copyField(const Type &type);
    void generateFieldSource(const Expression &expression);
    void generateFieldLet(const LetStatement &statement);
    void generateFieldAssignment(const AssignmentStatement &assignment);
    void copyAssignedFieldParameters(const std::vector<Parameter> &parameters, const BlockStatement *body);

    // every field element (i32, f32 or bool) takes 4 bytes
    static constexpr std::uint32_t FIELD_ELEMENT_BYTES = 4;
    // a v128 holds 4 elements. field blocks are padded to whole chunks
    static constexpr std::uint32_t CHUNK_BYTES = 16;
    void generateUnary(const UnaryExpr &unary);
    void generateBinary(const BinaryExpr &binary);

    // scope and naming helpers
    void enterScope();
    void exitScope();
    std::string declareLocal(const std::string &name, const Type &type);
    std::string declareLocal(const std::string &name, WatType type);
    std::string newLocal(const std::string &base, WatType type);
    std::string uniqueLocalName(const std::string &name);
    std::string declareParameter(const std::string &name);
    std::string lookupLocal(const std::string &name);

    // maps a LANES type to a WAT type if unsupported
    bool toWatType(const Type &type, WatType &result, const std::string &context);

    void addError(const std::string &message);
    void reset();
};

#endif // LANES_CODEGEN_HPP
