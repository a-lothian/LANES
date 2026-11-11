#ifndef LANES_WAT_EMITTER_HPP
#define LANES_WAT_EMITTER_HPP

#include <string>
#include <vector>

enum class WatType { I32, F32 };

struct WatParam
{
    std::string name;
    WatType type;
};

class WatFunction
{
public:
    WatFunction(std::string name, std::vector<WatParam> params, std::vector<WatType> results);



    // declare a local var
    // codegen will deal with reordering local definitions (must be at top)
    void addLocal(const std::string &name, WatType type);
    bool hasLocal(const std::string &name) const;

    // consts
    void i32Constant(int value);
    void f32Constant(float value);

    // variables
    void localGet(const std::string &name);
    void localSet(const std::string &name);
    void localTee(const std::string &name);
    void globalGet(const std::string &name);
    void globalSet(const std::string &name);

    // i32 operations
    void i32Addition();
    void i32Subtraction();
    void i32Multiplication();
    void i32SignedDivision();
    void i32Equal();
    void i32NotEqual();
    void i32SignedLessThan();
    void i32SignedGreaterThan();
    void i32SignedLessThanOrEqual();
    void i32SignedGreaterThanOrEqual();
    void i32EqualToZero();

    // f32 operations
    void f32Addition();
    void f32Subtraction();
    void f32Multiplication();
    void f32Division();
    void f32Negation();
    void f32Equal();
    void f32NotEqual();
    void f32LessThan();
    void f32GreaterThan();
    void f32LessThanOrEqual();
    void f32GreaterThanOrEqual();

    // conversions
    void f32ConvertFromSignedI32();
    void i32TruncateFromSignedF32();

    // stack and call management
    void call(const std::string &fn);
    void drop();
    void returnFromFunction();
    void unreachable();

    // control flow
    // labels are optional and name outputted WAT jumps
    void beginBlock(const std::string &label = "");
    void beginLoop(const std::string &label = "");
    void beginIf(const std::string &label = "");
    void beginElse();
    void end();
    void branch(const std::string &label);
    void branchIf(const std::string &label);

    // return the entire program as a string
    std::string str(int indent = 1) const;





};

#endif // LANES_WAT_EMITTER_HPP

class WatModule  // we will only use one module per program
{
   public:
    WatFunction& addFunction(const std::string& name, std::vector<WatParam> params,
                             std::vector<WatType> results);

    // WAT: (export "exportName" (func $fn));
    void exportFunction(const std::string& fn, const std::string& exportName = "");

    // WAT: (import "module" "name" (func $local (param ...) (result ...)))
    void importFunction(const std::string& module, const std::string& name,
                        const std::string& local, const std::vector<WatType>& params,
                        const std::vector<WatType>& results);

    // WAT: (global $name (mut) type (init const))
    void addGlobalI32(const std::string& name, int init, bool mut = false);
    void addGlobalF32(const std::string& name, float init, bool mut = false);

    // WAT: (memory (export "memory")? pages)
    // WASM pages are 64kb = 65536 b
    void setMemory(unsigned pages, bool exported = true);

    std::string str() const;  // debug

   private:
    std::vector<std::string> imports_;
    std::vector<std::string> globals_;
    std::vector<WatFunction> functions_;
    std::vector<std::string> exports_;
    std::string memory_;
};

#endif  // LANES_WAT_EMITTER_HPP