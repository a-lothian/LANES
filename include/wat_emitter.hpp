#ifndef LANES_WAT_EMITTER_HPP
#define LANES_WAT_EMITTER_HPP

#include <array>
#include <string>
#include <vector>

enum class WatType { I32, F32, V128 };
enum class LaneType { I32x4, F32x4 };

struct WatParam
{
    std::string name;
    WatType type;
};

class WatFunction
{
public:
    WatFunction(std::string name, std::vector<WatParam> params, std::vector<WatType> results);

    const std::string& name() const { return name_; }

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

    void f32Absolute();
    void f32Minimum();
    void f32Maximum();
    void f32SquareRoot();
    void f32Floor();
    void f32Ceiling();
    void f32Nearest();  // rounds half to even, e.g. 2.5 -> 2

    // pops a, b, condition -> a if condition is non-zero, else b
    void select();

    // conversions
    void f32ConvertFromSignedI32();
    void i32TruncateFromSignedF32();            // traps on NaN or out of range
    void i32SaturatingTruncateFromSignedF32();  // NaN -> 0, out of range -> INT_MIN / INT_MAX

    // memory management (for fields)
    // offste is a constant byte offset added to the address on the stack
    // load: pops an address, pushes the value stored there
    // store: pops a value and an address (address pushed first), writes the value to memory
    void i32LoadFromMemory(unsigned offset = 0);
    void i32StoreToMemory(unsigned offset = 0);
    void f32LoadFromMemory(unsigned offset = 0);
    void f32StoreToMemory(unsigned offset = 0);

    void memoryCopy();
    void v128LoadFromMemory(unsigned offset = 0);
    void v128StoreToMemory(unsigned offset = 0);

    // SIMD operations

    // constants
    void v128ConstantI32(const std::array<int, 4> &lanes);
    void v128ConstantF32(const std::array<float, 4> &lanes);
    void vectorSplat(LaneType lanes);                       // scalar -> the same value in every lane
    void vectorExtractLane(LaneType lanes, unsigned lane);  // v128 -> the scalar in one lane (0-3)
    // pops a, b -> a v128 built from 32 bit lanes of a (0-3) and b (4-7),
    // e.g. {3, 2, 5, 4} gives [a3, a2, b1, b0]
    void vectorShuffle(const std::array<unsigned, 4> &lanes);

    // arithmetic
    void vectorAddition(LaneType lanes);
    void vectorSubtraction(LaneType lanes);
    void vectorMultiplication(LaneType lanes);
    void vectorNegation(LaneType lanes);
    void vectorAbsolute(LaneType lanes);
    void vectorMinimum(LaneType lanes);  // signed for i32x4
    void vectorMaximum(LaneType lanes);  // signed for i32x4
    void f32x4Division();
    void f32x4SquareRoot();
    void f32x4Floor();
    void f32x4Ceiling();
    void f32x4Nearest();                 // rounds half to even, e.g. 2.5 -> 2

    void vectorEqual(LaneType lanes);
    void vectorNotEqual(LaneType lanes);
    void vectorLessThan(LaneType lanes);  // signed for i32x4, as are the three below
    void vectorGreaterThan(LaneType lanes);
    void vectorLessThanOrEqual(LaneType lanes);
    void vectorGreaterThanOrEqual(LaneType lanes);

    // conversions
    void tof32x4();  // i32x4 -> f32x4
    void toi32x4();  // f32x4 -> i32x4, truncates toward zero,

    // bitwise operations on all 128 bits
    void v128And();
    void v128Or();
    void v128Not();
    void v128BitSelect();  // pops a, b, mask -> bits of a where mask is 1, bits of b where it is 0
    void v128AnyTrue();    // -> i32 1 if any bit is set
    void i32x4AllTrue();   // -> i32 1 if every lane is non-zero

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
    // an if that leaves one value on the stack: both branches must push it
    void beginIfWithResult(WatType result);
    void beginElse();
    void end();
    void branch(const std::string &label);
    void branchIf(const std::string &label);

    // return the entire program as a string
    std::string str(int indent = 1) const;

   private:
    void emit(const std::string& instr);                              // write WAT line
    void open(const std::string& keyword, const std::string& label);  // step into a control section (depth++)

    std::string name_;
    std::vector<WatParam> params_;
    std::vector<WatType> results_;
    std::vector<WatParam> locals_;
    std::vector<std::string> lines_;  // WAT program
    int depth_ = 0;                   // how many [functions | loops | if statements] deep am I?
};

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