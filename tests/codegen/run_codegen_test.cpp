#include <iostream>
#include <string>
#include <vector>
#include "codegen.hpp"
#include "file_handler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include "run_codegen_test.h"

// each test compiles tests/codegen/generator/input/<name> and compares the output with
// tests/codegen/generator/validation/<name>
// the expected file holds either the generated WAT, or one "stage: message" line per error
static std::vector<std::string> tests = {
    "add_function.txt", "while_loop.txt", "if_else_chain.txt",
    "float_arithmetic.txt", "shadowing.txt", "calls.txt",
    "bool_comparison.txt", "nested_loops.txt", "ruleset_unsupported.txt",
    "field_literal.txt"};

static FileHandler reader;

// runs the whole compiler on a source string and returns the WAT, or the errors as text
static std::string compile(const std::string &source)
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
            std::string errors;
            for (const std::string &error : analyser.getErrors())
            {
                errors += "semantic: " + error + "\n";
            }
            return errors;
        }

        CodeGenerator generator;
        if (!generator.generate(program))
        {
            std::string errors;
            for (const std::string &error : generator.getErrors())
            {
                errors += "codegen: " + error + "\n";
            }
            return errors;
        }

        return generator.getWat();
    }
    catch (const ParseError &error)
    {
        return std::string("parse: ") + error.what() + "\n";
    }
}

bool run_codegen_tests()
{
    bool passing = true;
    for (const std::string &filename : tests)
    {
        if (!reader.readFile(R"(tests/codegen/generator/input/)" + filename))
        {
            std::cout << "FAILED: " << filename << "<------| (input file missing)\n";
            passing = false;
            continue;
        }
        std::string test_output = compile(reader.document);

        if (!reader.readFile(R"(tests/codegen/generator/validation/)" + filename))
        {
            std::cout << "FAILED: " << filename << "<------| (validation file missing)\n";
            passing = false;
            continue;
        }
        std::string test_validation = reader.document;

        if (test_validation == test_output)
        {
            std::cout << "PASSED: " << filename << "\n";
        }
        else
        {
            std::cout << "FAILED: " << filename << "<------|\n";
            std::cout << "    Expected output : " << test_validation << "\n";
            std::cout << "    Generator output: " << test_output << "\n";
            passing = false;
        }
    }
    std::cout << "Codegen Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
