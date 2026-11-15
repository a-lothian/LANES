#include "run_exec_test.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "codegen.hpp"
#include "file_handler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

static std::vector<std::string> tests = {
    "print_scalars.txt", "control_flow.txt", "field_print.txt", "field_values.txt", "rulesets.txt", "ruleset_simd.txt", "reductions.txt"};

static const std::string BUILD_DIRECTORY = "build/exec/";

// runs the whole compiler, returns false and the errors as text if it fails
static bool compile(const std::string &source, std::string &output)
{
    Lexer lexer(source);
    lexer.tokenize();

    try
    {
        Parser parser(lexer.get_tokens(), source);
        Program program = parser.parse();

        SemanticAnalyser analyser;
        if (!analyser.analyse(program))
        {
            output.clear();
            for (const std::string &error : analyser.getErrors())
            {
                output += "semantic: " + error + "\n";
            }
            return false;
        }

        CodeGenerator generator;
        if (!generator.generate(program))
        {
            output.clear();
            for (const std::string &error : generator.getErrors())
            {
                output += "codegen: " + error + "\n";
            }
            return false;
        }

        output = generator.getWat();
        return true;
    }
    catch (const ParseError &error)
    {
        output = std::string("parse: ") + error.what() + "\n";
        return false;
    }
}

// runs a shell command, returns its exit status and everything it printed
static int runCommand(const std::string &command, std::string &output)
{
    output.clear();
    FILE *pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == nullptr)
    {
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output += buffer;
    }
    return pclose(pipe);
}

static bool commandExists(const std::string &name)
{
    return std::system(("command -v " + name + " >/dev/null 2>&1").c_str()) == 0;
}

static std::string runTest(const std::string &source, const std::string &name)
{
    std::string wat;
    if (!compile(source, wat))
    {
        return wat;
    }

    FileHandler files;
    std::string base = BUILD_DIRECTORY + name.substr(0, name.find_last_of('.'));
    if (!files.writeFile(base + ".wat", wat))
    {
        return "write: " + files.getError() + "\n";
    }

    std::string output;
    if (runCommand("wat2wasm \"" + base + ".wat\" -o \"" + base + ".wasm\"", output) != 0)
    {
        return "wat2wasm: " + output;
    }

    runCommand("node tools/run_wasm.mjs \"" + base + ".wasm\"", output);
    return output;
}

bool runExecTests()
{
    if (!commandExists("node") || !commandExists("wat2wasm"))
    {
        std::cout << "SKIPPED: execution tests need node and wat2wasm (see 'make install')\n";
        std::cout << "Execution Test Result: SKIPPED\n";
        return true;
    }

    std::filesystem::create_directories(BUILD_DIRECTORY);

    FileHandler reader;
    bool passing = true;

    for (const std::string &filename : tests)
    {
        if (!reader.readFile("tests/exec/input/" + filename))
        {
            std::cout << "FAILED: " << filename << "<------| (input file missing)\n";
            passing = false;
            continue;
        }
        std::string output = runTest(reader.document, filename);

        if (!reader.readFile("tests/exec/expected/" + filename))
        {
            std::cout << "FAILED: " << filename << "<------| (expected file missing)\n";
            passing = false;
            continue;
        }

        if (output == reader.document)
        {
            std::cout << "PASSED: " << filename << "\n";
        }
        else
        {
            std::cout << "FAILED: " << filename << "<------|\n";
            std::cout << "    Expected output:\n" << reader.document << "\n";
            std::cout << "    Program output:\n" << output << "\n";
            passing = false;
        }
    }

    std::cout << "Execution Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
