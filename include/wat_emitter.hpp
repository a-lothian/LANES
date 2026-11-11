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

    // return the entire program as a string
    std::string str();
};

#endif // LANES_WAT_EMITTER_HPP
