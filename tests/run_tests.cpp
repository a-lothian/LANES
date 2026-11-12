#include <iostream>
#include "lexer/run_lexer_test.h"
#include "parser/run_parser_test.h"
#include "codegen/run_wat_test.h"
#include "semantic/run_symbol_table_test.h"

int main()
{
    bool passing = true;

    passing &= run_lexer_tests();
    passing &= run_parser_tests();
    passing &= run_wat_tests();
    passing &= runSymbolTableTests();

    std::cout << "\nOverall Result: " << (passing ? "PASSED" : "FAILED") << "\n";

    return passing ? 0 : 1;
}