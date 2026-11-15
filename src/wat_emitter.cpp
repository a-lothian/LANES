#include "wat_emitter.hpp"

#include <cmath>
#include <sstream>

// ensure float precision
#include <iomanip>
#include <limits>

const char* watTypeName(WatType t) {
    switch (t) {
    case WatType::I32:
        return "i32";
    case WatType::F32:
        return "f32";
    }
    return "i32";
}

// utils
static std::string f32Literal(float v) {
    if (std::isnan(v))
        return "nan";
    if (std::isinf(v))
        return v < 0 ? "-inf" : "inf";
    std::ostringstream os;
    os << std::setprecision(std::numeric_limits<float>::max_digits10) << v;
    return os.str();
}

static std::string memarg(const char* op, unsigned offset) {
    std::string s = op;
    if (offset != 0)
        s += " offset=" + std::to_string(offset);
    return s;
}

// for defining functions : write result type in WAT format
// e.g `(result i32)`
static std::string resultClause(const std::vector<WatType>& results) {
    std::string s;
    for (WatType t : results)
        s += std::string(" (result ") + watTypeName(t) + ")";
    return s;
}

// for defining functions : write parameter type in WAT format
// e.g `(param i32) (param f32)`
static std::string paramClause(const std::vector<WatType>& params) {
    std::string s;
    for (WatType t : params)
        s += std::string(" (param ") + watTypeName(t) + ")";
    return s;
}



WatFunction::WatFunction(std::string name, std::vector<WatParam> params, std::vector<WatType> results)
    : name_(std::move(name)), params_(std::move(params)), results_(std::move(results)) {}



void WatFunction::addLocal(const std::string& name, WatType type) {
    if (!hasLocal(name))
        locals_.push_back({name, type});
}

bool WatFunction::hasLocal(const std::string& name) const {
    for (const auto& l : locals_)
        if (l.name == name)
            return true;
    for (const auto& p : params_)
        if (p.name == name)
            return true;
    return false;
}

void WatFunction::emit(const std::string& instr) {
    lines_.push_back(std::string(depth_ * 2, ' ') + instr);
}



void WatFunction::i32Constant(int value) { emit("i32.const " + std::to_string(value)); }
void WatFunction::f32Constant(float value) { emit("f32.const " + f32Literal(value)); }


void WatFunction::localGet(const std::string& name) { emit("local.get $" + name); }
void WatFunction::localSet(const std::string& name) { emit("local.set $" + name); }
void WatFunction::localTee(const std::string& name) { emit("local.tee $" + name); }
void WatFunction::globalGet(const std::string& name) { emit("global.get $" + name); }
void WatFunction::globalSet(const std::string& name) { emit("global.set $" + name); }


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


void WatFunction::i32LoadFromMemory(unsigned offset) { emit(memarg("i32.load", offset)); }
void WatFunction::i32StoreToMemory(unsigned offset) { emit(memarg("i32.store", offset)); }
void WatFunction::f32LoadFromMemory(unsigned offset) { emit(memarg("f32.load", offset)); }
void WatFunction::f32StoreToMemory(unsigned offset) { emit(memarg("f32.store", offset)); }
void WatFunction::memoryCopy() { emit("memory.copy"); }


void WatFunction::call(const std::string& functionName) { emit("call $" + functionName); }
void WatFunction::drop() { emit("drop"); }
void WatFunction::returnFromFunction() { emit("return"); }
void WatFunction::unreachable() { emit("unreachable"); }


void WatFunction::open(const std::string& keyword, const std::string& label) {
    emit(label.empty() ? keyword : keyword + " $" + label);
    ++depth_;
}

void WatFunction::beginBlock(const std::string& label) { open("block", label); }
void WatFunction::beginLoop(const std::string& label) { open("loop", label); }
void WatFunction::beginIf(const std::string& label) { open("if", label); }


void WatFunction::beginElse() {
    if (depth_ == 0)
        throw std::logic_error("WatFunction::beginElse with no open construct");
    --depth_;
    emit("else");
    ++depth_;
}

void WatFunction::end() {
    if (depth_ == 0)
        throw std::logic_error("WatFunction::end with no open construct");
    --depth_;
    emit("end");
}


void WatFunction::branch(const std::string& label) { emit("br $" + label); }
void WatFunction::branchIf(const std::string& label) { emit("br_if $" + label); }


std::string WatFunction::str(int indent) const {
    if (depth_ != 0)
        throw std::logic_error("WatFunction '" + name_ + "' has unclosed control-flow constructs");

    const std::string pad(indent * 2, ' ');
    std::ostringstream os;
    os << pad << "(func $" << name_;
    for (const auto& p : params_)
        os << " (param $" << p.name << " " << watTypeName(p.type) << ")";
    os << resultClause(results_) << "\n";
    for (const auto& l : locals_)
        os << pad << "  (local $" << l.name << " " << watTypeName(l.type) << ")\n";
    for (const auto& line : lines_)
        os << pad << "  " << line << "\n";
    os << pad << ")";
    return os.str();
}

// WAT MODULE

WatFunction& WatModule::addFunction(const std::string& name, std::vector<WatParam> params,
                                    std::vector<WatType> results) {
    functions_.emplace_back(name, std::move(params), std::move(results));
    return functions_.back();
}

void WatModule::exportFunction(const std::string& fn, const std::string& exportName) {
    exports_.push_back("(export \"" + (exportName.empty() ? fn : exportName) + "\" (func $" + fn + "))");
}

void WatModule::importFunction(const std::string& module, const std::string& name,
                               const std::string& local, const std::vector<WatType>& params,
                               const std::vector<WatType>& results) {
    imports_.push_back("(import \"" + module + "\" \"" + name + "\" (func $" + local +
                       paramClause(params) + resultClause(results) + "))");
}

void WatModule::addGlobalI32(const std::string& name, int init, bool mut) {
    globals_.push_back("(global $" + name + " " + (mut ? "(mut i32)" : "i32") +
                       " (i32.const " + std::to_string(init) + "))");
}

void WatModule::addGlobalF32(const std::string& name, float init, bool mut) {
    globals_.push_back("(global $" + name + " " + (mut ? "(mut f32)" : "f32") +
                       " (f32.const " + f32Literal(init) + "))");
}

void WatModule::setMemory(unsigned pages, bool exported) {
    memory_ = std::string("(memory ") + (exported ? "(export \"memory\") " : "") + std::to_string(pages) + ")";
}

std::string WatModule::str() const {
    std::ostringstream os;
    os << "(module\n";
    for (const auto& s : imports_)
        os << "  " << s << "\n";
    if (!memory_.empty())
        os << "  " << memory_ << "\n";
    for (const auto& s : globals_)
        os << "  " << s << "\n";
    for (const auto& f : functions_)
        os << f.str(1) << "\n";
    for (const auto& s : exports_)
        os << "  " << s << "\n";
    os << ")\n";
    return os.str();
}
