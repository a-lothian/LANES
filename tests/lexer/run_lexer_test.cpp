#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "lexer.hpp"
#include "file_handler.hpp"
#include "run_lexer_test.h"

static std::vector<std::string> tests = {
    "alltokens.txt", "empty.txt", "errors.txt",
    "expression.txt", "greedy.txt", "number_leading_dot.txt",
    "number_trailing_dot.txt", "number_two_dots.txt", "numbers.txt",
    "singlechartokens.txt", "twochartokens.txt",
    "keywords.txt", "names.txt", "keyword_prefix.txt", "keyword_case.txt",
    "name_number_mix.txt", "keyword_expression.txt", "keyword_punctuation.txt",
    "control_keywords.txt", "control_keyword_prefix.txt", "control_keyword_expression.txt",
    "whitespace_basic.txt", "whitespace_only.txt", "whitespace_splits_tokens.txt", 
    "whitespace_splits_operators.txt", "comment_basic.txt", "comment_at_eof.txt", 
    "comment_only.txt", "comment_lines.txt", "comment_slash.txt", 
    "comment_contents.txt", "comment_triple_slash.txt", "comment_adjacent.txt", 
    "comment_crlf.txt", "program.txt", "program_long.txt"
};

static FileHandler reader;
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

        if (!reader.readFile(R"(tests/lexer/input/)"  + filename)) {
            std::cout << "FAILED: " << filename << "<------| (" << reader.getError() << ")\n";
            passing = false;
            continue;
        }
        test_input = reader.document;

        lexer.tokenize(test_input);
        for (Token t : lexer.get_tokens()){
            ss << tokenSymbol(t.type);
        }
        test_output = ss.str();
        if (!reader.readFile(R"(tests/lexer/validation/)" + filename)) {
            std::cout << "FAILED: " << filename << "<------| (" << reader.getError() << ")\n";
            passing = false;
            continue;
        }
        test_validation = reader.document;

        bool test_passed = test_validation.compare(test_output) == 0;

        if (test_passed) {
            std::cout << "PASSED: " << filename << "\n";
        } else {
            std::cout << "FAILED: " << filename << "<------|\n";
            std::cout << "    Expected output : " << test_validation << "\n";
            std::cout << "    Lexer output: " << test_output << "\n";
            passing = false;
        }
    }
    std::cout << "Lexer Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}