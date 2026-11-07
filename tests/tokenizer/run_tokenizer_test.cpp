#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "lexer.h"
#include "file_reader.hpp"

std::vector<std::string> tests = {
    "alltokens.txt", "empty.txt", "errors.txt",
    "expression.txt", "greedy.txt", "number_leading_dot.txt",
    "number_trailing_dot.txt", "number_two_dots.txt", "numbers.txt",
    "singlechartokens.txt", "twochartokens.txt"
};

FileReader reader;
Lexer lexer;

std::string test_input;
std::string test_output;
std::string test_validation;

std::ostringstream ss;

static void clearData() {
    test_input.clear();
    test_output.clear();
    test_validation.clear();
    ss.str("");
    ss.clear();
}

int main() {
    bool passing = true;
    for (std::string filename : tests) {
        clearData();

        reader.dumpFile(R"(tests/tokenizer/input/)"  + filename);
        test_input = reader.document;

        lexer.tokenize(test_input);
        for (Token t : lexer.tokens()){
            ss << tokenSymbol(t.type);
        }
        test_output = ss.str();
        reader.dumpFile(R"(tests/tokenizer/validation/)" + filename);
        test_validation = reader.document;

        bool test_passed = test_validation.compare(test_output) == 0;

        if (test_passed) {
            std::cout << "PASSED: " << filename << "\n";
        } else {
            std::cout << "FAILED: " << filename << "\n";
            std::cout << "    Expected output : " << test_validation << "\n";
            std::cout << "    Tokenizer output: " << test_output << "\n";
            passing = false;
        }
    }
    std::cout << "Tokenizer Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
}