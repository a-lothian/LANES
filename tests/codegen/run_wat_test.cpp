#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "file_handler.hpp"
#include "wat_emitter.hpp"
#include "memory_layout.hpp"
#include "run_wat_test.h"

// each test manually builds a program and compares output against
// a hand-written reference in tests/codegen/validation

// basic add function
static std::string buildAddFunction() {
    WatModule m;
    auto &f = m.addFunction("add", {{"x", WatType::I32}, {"y", WatType::I32}}, {WatType::I32});
    f.addLocal("result", WatType::I32);
    f.localGet("x");
    f.localGet("y");
    f.i32Addition();
    f.localSet("result");
    f.localGet("result");
    m.exportFunction("add");
    return m.str();
}

// multiply two floats
static std::string buildFloatConstant() {
    WatModule m;
    auto &f = m.addFunction("fl", {}, {WatType::F32});
    f.f32Constant(123.456f);
    f.f32Constant(2.0f);
    f.f32Multiplication();
    return m.str();
}

// increment an integer in a for-style loop
static std::string buildLoop() {
    WatModule m;
    auto &f = m.addFunction("sum", {{"n", WatType::I32}}, {WatType::I32});
    f.addLocal("i", WatType::I32);
    f.addLocal("acc", WatType::I32);
    f.beginBlock("done");
    f.beginLoop("again");
    f.localGet("i");
    f.localGet("n");
    f.i32SignedGreaterThanOrEqual();
    f.branchIf("done");
    f.localGet("acc");
    f.localGet("i");
    f.i32Addition();
    f.localSet("acc");
    f.localGet("i");
    f.i32Constant(1);
    f.i32Addition();
    f.localSet("i");
    f.branch("again");
    f.end();
    f.end();
    f.localGet("acc");
    return m.str();
}

// test if branching logic
static std::string buildModuleSectionsIfElse() {
    WatModule m;
    m.setMemory(1);
    m.importFunction("env", "print", "print", {WatType::I32}, {});
    m.addGlobalI32("g", 7, true);
    auto &f = m.addFunction("pick", {{"c", WatType::I32}}, {WatType::F32});
    f.localGet("c");
    f.beginIf();
    f.f32Constant(1.0f);
    f.returnFromFunction();
    f.beginElse();
    f.f32Constant(0.5f);
    f.f32Negation();
    f.returnFromFunction();
    f.end();
    f.unreachable();
    f.i32LoadFromMemory(4);
    return m.str();
}

// returns the exception message if str() fails
static std::string buildUnclosedBlock() {
    WatModule m;
    auto &f = m.addFunction("bad", {}, {});
    f.beginBlock();
    try {
        m.str();
    } catch (const std::logic_error &e) {
        return e.what();
    }
    return "no exception";
}

// reserves blocks at compile time and sizes a module's memory to fit them
// e.g. 16 bytes = field<f32, 4>, 20 bytes = field<f32, 5>
static std::string buildMemoryLayout() {
    MemoryLayout layout;
    std::string out;

    auto reserve = [&](std::uint64_t bytes) {
        std::uint32_t address = 0;
        bool ok = layout.reserve(bytes, address);
        out += "reserve " + std::to_string(bytes) + " -> " +
               (ok ? std::to_string(address) : std::string("too large")) + "\n";
    };

    out += "empty: " + std::string(layout.empty() ? "yes" : "no") + "\n";
    reserve(16);          // starts at 0
    reserve(20);          // next 16 byte boundary
    reserve(4);           // 16 + 20 = 36, rounded up to 48
    reserve(70000);       // crosses into a second page
    reserve(4294967296);  // 4gb on top of what is already reserved
    out += "size: " + std::to_string(layout.size()) + "\n";
    out += "pages: " + std::to_string(layout.pages()) + "\n";

    WatModule m;
    m.setMemory(layout.pages());
    return out + m.str();
}

// every SIMD instruction, with each result stored in its own 16 byte slot
// slot k is at address 16 * k
static std::string buildSimd() {
    WatModule m;
    m.setMemory(1);
    auto &f = m.addFunction("simd", {{"s", WatType::F32}, {"n", WatType::I32}}, {WatType::F32});
    f.addLocal("v", WatType::V128);

    const std::array<float, 4> a = {1.5f, -2.0f, 2.5f, -0.5f};
    const std::array<float, 4> b = {2.0f, 4.0f, 0.5f, -0.5f};
    const std::array<int, 4> i = {1, -2, 3, -4};
    const std::array<int, 4> j = {5, 6, -7, -4};

    int slot = 0;
    auto nextSlot = [&]() { f.i32Constant(16 * slot++); };

    using LaneOp = void (WatFunction::*)(LaneType);
    using Op = void (WatFunction::*)();

    // slot = a <op> b, or i <op> j
    auto binaryF = [&](LaneOp op) {
        nextSlot(); f.v128ConstantF32(a); f.v128ConstantF32(b); (f.*op)(LaneType::F32x4); f.v128StoreToMemory();
    };
    auto binaryI = [&](LaneOp op) {
        nextSlot(); f.v128ConstantI32(i); f.v128ConstantI32(j); (f.*op)(LaneType::I32x4); f.v128StoreToMemory();
    };

    // slots 0-11: f32x4 a <op> b
    for (LaneOp op : {&WatFunction::vectorAddition, &WatFunction::vectorSubtraction,
                      &WatFunction::vectorMultiplication, &WatFunction::vectorMinimum,
                      &WatFunction::vectorMaximum, &WatFunction::vectorEqual,
                      &WatFunction::vectorNotEqual, &WatFunction::vectorLessThan,
                      &WatFunction::vectorGreaterThan, &WatFunction::vectorLessThanOrEqual,
                      &WatFunction::vectorGreaterThanOrEqual})
        binaryF(op);
    nextSlot(); f.v128ConstantF32(a); f.v128ConstantF32(b); f.f32x4Division(); f.v128StoreToMemory();

    // slots 12-17: f32x4 <op> a
    for (LaneOp op : {&WatFunction::vectorNegation, &WatFunction::vectorAbsolute}) {
        nextSlot(); f.v128ConstantF32(a); (f.*op)(LaneType::F32x4); f.v128StoreToMemory();
    }
    for (Op op : {&WatFunction::f32x4Floor, &WatFunction::f32x4Ceiling, &WatFunction::f32x4Nearest}) {
        nextSlot(); f.v128ConstantF32(a); (f.*op)(); f.v128StoreToMemory();
    }
    nextSlot(); f.v128ConstantF32({4.0f, 9.0f, 0.25f, 2.0f}); f.f32x4SquareRoot(); f.v128StoreToMemory();

    // slots 18-28: i32x4 i <op> j
    for (LaneOp op : {&WatFunction::vectorAddition, &WatFunction::vectorSubtraction,
                      &WatFunction::vectorMultiplication, &WatFunction::vectorMinimum,
                      &WatFunction::vectorMaximum, &WatFunction::vectorEqual,
                      &WatFunction::vectorNotEqual, &WatFunction::vectorLessThan,
                      &WatFunction::vectorGreaterThan, &WatFunction::vectorLessThanOrEqual,
                      &WatFunction::vectorGreaterThanOrEqual})
        binaryI(op);

    // slots 29-30: i32x4 <op> i
    for (LaneOp op : {&WatFunction::vectorNegation, &WatFunction::vectorAbsolute}) {
        nextSlot(); f.v128ConstantI32(i); (f.*op)(LaneType::I32x4); f.v128StoreToMemory();
    }

    // slots 31-34: splats and conversions
    nextSlot(); f.localGet("s"); f.vectorSplat(LaneType::F32x4); f.v128StoreToMemory();
    nextSlot(); f.localGet("n"); f.vectorSplat(LaneType::I32x4); f.v128StoreToMemory();
    nextSlot(); f.v128ConstantI32(i); f.tof32x4(); f.v128StoreToMemory();
    nextSlot(); f.v128ConstantF32(a); f.toi32x4(); f.v128StoreToMemory();

    // slots 35-38: bitwise, and bitselect(i, j, i < j)
    for (Op op : {&WatFunction::v128And, &WatFunction::v128Or}) {
        nextSlot(); f.v128ConstantI32(i); f.v128ConstantI32(j); (f.*op)(); f.v128StoreToMemory();
    }
    nextSlot(); f.v128ConstantI32(i); f.v128Not(); f.v128StoreToMemory();
    nextSlot();
    f.v128ConstantI32(i);
    f.v128ConstantI32(j);
    f.v128ConstantI32(i);
    f.v128ConstantI32(j);
    f.vectorLessThan(LaneType::I32x4);
    f.v128BitSelect();
    f.v128StoreToMemory();

    // slot 39: load slot 18 (i + j) through a v128 local and double it
    nextSlot();
    f.i32Constant(0);
    f.v128LoadFromMemory(16 * 18);
    f.localTee("v");
    f.localGet("v");
    f.vectorAddition(LaneType::I32x4);
    f.v128StoreToMemory();

    // slot 40: any_true / all_true as four i32s
    auto storeI32 = [&](unsigned offset) { f.i32StoreToMemory(offset); };
    int base = 16 * slot++;
    f.i32Constant(base); f.v128ConstantI32(i); f.v128ConstantI32(j); f.vectorLessThan(LaneType::I32x4); f.v128AnyTrue(); storeI32(0);
    f.i32Constant(base); f.v128ConstantI32({0, 0, 0, 0}); f.v128AnyTrue(); storeI32(4);
    f.i32Constant(base); f.v128ConstantI32(i); f.v128ConstantI32(j); f.vectorLessThan(LaneType::I32x4); f.i32x4AllTrue(); storeI32(8);
    f.i32Constant(base); f.v128ConstantI32(i); f.i32x4AllTrue(); storeI32(12);

    // slot 41: lane 3 of i as an i32, lane 1 of a as an f32
    base = 16 * slot++;
    f.i32Constant(base); f.v128ConstantI32(i); f.vectorExtractLane(LaneType::I32x4, 3); storeI32(0);
    f.i32Constant(base); f.v128ConstantF32(a); f.vectorExtractLane(LaneType::F32x4, 1); f.f32StoreToMemory(4);

    // result: lane 2 of a
    f.v128ConstantF32(a);
    f.vectorExtractLane(LaneType::F32x4, 2);

    m.exportFunction("simd");
    return m.str();
}

// lanes are numbered 0-3
static std::string buildBadLane() {
    WatModule m;
    auto &f = m.addFunction("bad", {}, {});
    try {
        f.vectorExtractLane(LaneType::I32x4, 4);
    } catch (const std::logic_error &e) {
        return e.what();
    }
    return "no exception";
}

struct WatTest {
    std::string filename;
    std::string (*build)();
};

static std::vector<WatTest> tests = {
    {"add_function.txt", buildAddFunction},
    {"float_constant.txt", buildFloatConstant},
    {"loop.txt", buildLoop},
    {"module_sections_if_else.txt", buildModuleSectionsIfElse},
    {"unclosed_block.txt", buildUnclosedBlock},
    {"memory_layout.txt", buildMemoryLayout},
    {"simd.txt", buildSimd},
    {"simd_bad_lane.txt", buildBadLane},
};

static FileHandler reader;

bool run_wat_tests() {
    bool passing = true;
    for (const WatTest &test : tests) {
        std::string test_output = test.build();

        if (!reader.readFile(R"(tests/codegen/validation/)" + test.filename)) {
            std::cout << "FAILED: " << test.filename << "<------| (validation file missing)\n";
            passing = false;
            continue;
        }
        std::string test_validation = reader.document;

        if (test_validation == test_output) {
            std::cout << "PASSED: " << test.filename << "\n";
        } else {
            std::cout << "FAILED: " << test.filename << "<------|\n";
            std::cout << "    Expected output : " << test_validation << "\n";
            std::cout << "    Emitter output: " << test_output << "\n";
            passing = false;
        }
    }
    std::cout << "WAT Emitter Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
