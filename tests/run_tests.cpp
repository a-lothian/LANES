#include <iostream>
#include "lexer/run_lexer_test.h"
#include "parser/run_parser_test.h"
#include "codegen/run_wat_test.h"
#include "codegen/run_codegen_test.h"
#include "semantic/run_symbol_table_test.h"
#include "semantic/run_control_flow_test.h"
#include "semantic/run_type_annotation_test.h"
#include "semantic/run_recursion_test.h"

int main()
{
    bool passing = true;

    passing &= run_lexer_tests();
    passing &= run_parser_tests();
    passing &= run_wat_tests();
    passing &= run_codegen_tests();
    passing &= runSymbolTableTests();
    passing &= runControlFlowSemanticTests();
    passing &= runTypeAnnotationTests();
    passing &= runRecursionTests();

    std::cout << "\nOverall Result: " << (passing ? "PASSED" : "FAILED") << "\n";

    return passing ? 0 : 1;
}