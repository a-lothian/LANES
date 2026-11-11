#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "file_reader.hpp"
#include "wat_emitter.hpp"
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
};

static FileReader reader;

bool run_wat_tests() {
    bool passing = true;
    for (const WatTest &test : tests) {
        std::string test_output = test.build();

        if (!reader.dumpFile(R"(tests/codegen/validation/)" + test.filename)) {
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
