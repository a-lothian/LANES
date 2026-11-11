#include <iostream>
#include "lexer/run_lexer_test.h"
#include "codegen/run_wat_test.h"

int main() {
    bool passing = true;

    passing &= run_lexer_tests();
    passing &= run_wat_tests();

    std::cout << "\nOverall Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing ? 0 : 1;
}
