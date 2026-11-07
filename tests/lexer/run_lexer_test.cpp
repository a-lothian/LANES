#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "lexer.hpp"
#include "file_reader.hpp"
#include "run_lexer_test.h"

static std::vector<std::string> tests = {
    "alltokens.txt", "empty.txt", "errors.txt",
    "expression.txt", "greedy.txt", "number_leading_dot.txt",
    "number_trailing_dot.txt", "number_two_dots.txt", "numbers.txt",
    "singlechartokens.txt", "twochartokens.txt",
    "keywords.txt", "names.txt", "keyword_prefix.txt", "keyword_case.txt",
    "name_number_mix.txt", "keyword_expression.txt", "keyword_punctuation.txt"
};

static FileReader reader;
static Lexer lexer;

static std::string test_input;
static std::string test_output;
static std::string test_validation;

static std::ostringstream ss;

static void clearData() {
    test_input.clear();
    test_output.clear();
    test_validation.clear();
    ss.str("");
    ss.clear();
}

bool run_lexer_tests() {
    bool passing = true;
    for (std::string filename : tests) {
        clearData();

        reader.dumpFile(R"(tests/lexer/input/)"  + filename);
        test_input = reader.document;

        lexer.tokenize(test_input);
        for (Token t : lexer.get_tokens()){
            ss << tokenSymbol(t.type);
        }
        test_output = ss.str();
        reader.dumpFile(R"(tests/lexer/validation/)" + filename);
        test_validation = reader.document;

        bool test_passed = test_validation.compare(test_output) == 0;

        if (test_passed) {
            std::cout << "PASSED: " << filename << "\n";
        } else {
            std::cout << "FAILED: " << filename << "\n";
            std::cout << "    Expected output : " << test_validation << "\n";
            std::cout << "    Lexer output: " << test_output << "\n";
            passing = false;
        }
    }
    std::cout << "Lexer Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}