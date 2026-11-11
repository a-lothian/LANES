#include "wat_emitter.hpp"

#include <iomanip>
#include <sstream>

WatFunction::WatFunction(std::string name, std::vector<WatParam> params, std::vector<WatType> results)
    : name_(std::move(name)), params_(std::move(params)), results_(std::move(results)) {}

void WatFunction::addLocal(const std::string &name, WatType type)
{
    if (!hasLocal(name))
        locals_.push_back({name, type});
}

bool WatFunction::hasLocal(const std::string &name) const
{
    for (const auto &l : locals_)
        if (l.name == name)
            return true;
    for (const auto &p : params_)
        if (p.name == name)
            return true;
    return false;
}

void WatFunction::emit(const std::string &instr)
{
    lines_.push_back(std::string(depth_ * 2, ' ') + instr);
}

void WatFunction::i32Constant(int value) { emit("i32.const " + std::to_string(value)); }
void WatFunction::f32Constant(float value) { emit("f32.const " + str(value)); }

void WatFunction::localGet(const std::string &name) { emit("local.get $" + name); }
void WatFunction::localSet(const std::string &name) { emit("local.set $" + name); }
void WatFunction::localTee(const std::string &name) { emit("local.tee $" + name); }
void WatFunction::globalGet(const std::string &name) { emit("global.get $" + name); }
void WatFunction::globalSet(const std::string &name) { emit("global.set $" + name); }

void WatFunction::i32Addition() { emit("i32.add"); }
void WatFunction::i32Subtraction() { emit("i32.sub"); }
void WatFunction::i32Multiplication() { emit("i32.mul"); }
void WatFunction::i32SignedDivision() { emit("i32.div_s"); }
void WatFunction::i32Equal() { emit("i32.eq"); }
void WatFunction::i32NotEqual() { emit("i32.ne"); }
void WatFunction::i32SignedLessThan() { emit("i32.lt_s"); }
void WatFunction::i32SignedGreaterThan() { emit("i32.gt_s"); }
void WatFunction::i32SignedLessThanOrEqual() { emit("i32.le_s"); }
void WatFunction::i32SignedGreaterThanOrEqual() { emit("i32.ge_s"); }
void WatFunction::i32EqualToZero() { emit("i32.eqz"); }

void WatFunction::f32Addition() { emit("f32.add"); }
void WatFunction::f32Subtraction() { emit("f32.sub"); }
void WatFunction::f32Multiplication() { emit("f32.mul"); }
void WatFunction::f32Division() { emit("f32.div"); }
void WatFunction::f32Negation() { emit("f32.neg"); }
void WatFunction::f32Equal() { emit("f32.eq"); }
void WatFunction::f32NotEqual() { emit("f32.ne"); }
void WatFunction::f32LessThan() { emit("f32.lt"); }
void WatFunction::f32GreaterThan() { emit("f32.gt"); }
void WatFunction::f32LessThanOrEqual() { emit("f32.le"); }
void WatFunction::f32GreaterThanOrEqual() { emit("f32.ge"); }

void WatFunction::f32ConvertFromSignedI32() { emit("f32.convert_i32_s"); }
void WatFunction::i32TruncateFromSignedF32() { emit("i32.trunc_f32_s"); }

void WatFunction::call(const std::string &functionName) { emit("call $" + functionName); }
void WatFunction::drop() { emit("drop"); }
void WatFunction::returnFromFunction() { emit("return"); }
void WatFunction::unreachable() { emit("unreachable"); }

void WatFunction::open(const std::string &keyword, const std::string &label)
{
    emit(label.empty() ? keyword : keyword + " $" + label);
    ++depth_;
}

void WatFunction::beginBlock(const std::string &label) { open("block", label); }
void WatFunction::beginLoop(const std::string &label) { open("loop", label); }
void WatFunction::beginIf(const std::string &label) { open("if", label); }

void WatFunction::beginElse()
{
    if (depth_ == 0)
        throw std::logic_error("WatFunction::beginElse with no open construct");
    --depth_;
    emit("else");
    ++depth_;
}

void WatFunction::end()
{
    if (depth_ == 0)
        throw std::logic_error("WatFunction::end with no open construct");
    --depth_;
    emit("end");
}

void WatFunction::branch(const std::string &label) { emit("br $" + label); }
void WatFunction::branchIf(const std::string &label) { emit("br_if $" + label); }
