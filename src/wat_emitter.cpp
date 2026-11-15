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
    case WatType::V128:
        return "v128";
    }
    return "i32";
}

// prefix of SIMD instructions, e.g. "i32x4" in i32x4.add
static std::string laneName(LaneType lanes) {
    return lanes == LaneType::I32x4 ? "i32x4" : "f32x4";
}

// instructions where the i32x4 version is signed, e.g. i32x4.lt_s vs f32x4.lt
static std::string signedOp(LaneType lanes, const std::string& op) {
    return laneName(lanes) + "." + op + (lanes == LaneType::I32x4 ? "_s" : "");
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
void WatFunction::v128LoadFromMemory(unsigned offset) { emit(memarg("v128.load", offset)); }
void WatFunction::v128StoreToMemory(unsigned offset) { emit(memarg("v128.store", offset)); }


void WatFunction::v128ConstantI32(const std::array<int, 4>& lanes) {
    std::string s = "v128.const i32x4";
    for (int v : lanes)
        s += " " + std::to_string(v);
    emit(s);
}

void WatFunction::v128ConstantF32(const std::array<float, 4>& lanes) {
    std::string s = "v128.const f32x4";
    for (float v : lanes)
        s += " " + f32Literal(v);
    emit(s);
}

void WatFunction::vectorSplat(LaneType lanes) { emit(laneName(lanes) + ".splat"); }

void WatFunction::vectorExtractLane(LaneType lanes, unsigned lane) {
    if (lane > 3)
        throw std::logic_error("vectorExtractLane: lane " + std::to_string(lane) + " is out of range");
    emit(laneName(lanes) + ".extract_lane " + std::to_string(lane));
}

// WAT only has a byte shuffle, so 32 bit lane k becomes bytes 4k .. 4k+3
void WatFunction::vectorShuffle(const std::array<unsigned, 4>& lanes) {
    std::string s = "i8x16.shuffle";
    for (unsigned lane : lanes) {
        if (lane > 7)
            throw std::logic_error("vectorShuffle: lane " + std::to_string(lane) + " is out of range");
        for (unsigned byte = 0; byte < 4; byte++)
            s += " " + std::to_string(lane * 4 + byte);
    }
    emit(s);
}

void WatFunction::vectorAddition(LaneType lanes) { emit(laneName(lanes) + ".add"); }
void WatFunction::vectorSubtraction(LaneType lanes) { emit(laneName(lanes) + ".sub"); }
void WatFunction::vectorMultiplication(LaneType lanes) { emit(laneName(lanes) + ".mul"); }
void WatFunction::vectorNegation(LaneType lanes) { emit(laneName(lanes) + ".neg"); }
void WatFunction::vectorAbsolute(LaneType lanes) { emit(laneName(lanes) + ".abs"); }
void WatFunction::vectorMinimum(LaneType lanes) { emit(signedOp(lanes, "min")); }
void WatFunction::vectorMaximum(LaneType lanes) { emit(signedOp(lanes, "max")); }
void WatFunction::f32x4Division() { emit("f32x4.div"); }
void WatFunction::f32x4SquareRoot() { emit("f32x4.sqrt"); }
void WatFunction::f32x4Floor() { emit("f32x4.floor"); }
void WatFunction::f32x4Ceiling() { emit("f32x4.ceil"); }
void WatFunction::f32x4Nearest() { emit("f32x4.nearest"); }

// eq and ne are the same for signed and unsigned, so have no _s
void WatFunction::vectorEqual(LaneType lanes) { emit(laneName(lanes) + ".eq"); }
void WatFunction::vectorNotEqual(LaneType lanes) { emit(laneName(lanes) + ".ne"); }
void WatFunction::vectorLessThan(LaneType lanes) { emit(signedOp(lanes, "lt")); }
void WatFunction::vectorGreaterThan(LaneType lanes) { emit(signedOp(lanes, "gt")); }
void WatFunction::vectorLessThanOrEqual(LaneType lanes) { emit(signedOp(lanes, "le")); }
void WatFunction::vectorGreaterThanOrEqual(LaneType lanes) { emit(signedOp(lanes, "ge")); }

void WatFunction::tof32x4() { emit("f32x4.convert_i32x4_s"); }
void WatFunction::toi32x4() { emit("i32x4.trunc_sat_f32x4_s"); }

void WatFunction::v128And() { emit("v128.and"); }
void WatFunction::v128Or() { emit("v128.or"); }
void WatFunction::v128Not() { emit("v128.not"); }
void WatFunction::v128BitSelect() { emit("v128.bitselect"); }
void WatFunction::v128AnyTrue() { emit("v128.any_true"); }
void WatFunction::i32x4AllTrue() { emit("i32x4.all_true"); }


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
