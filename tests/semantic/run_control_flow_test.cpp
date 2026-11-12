#include "run_control_flow_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>

// parses the source, runs semantic analysis and returns the error messages
// (valid programs return an empty list)
static std::vector<std::string> analyseSource(const std::string &body)
{
    std::string source = "function f(n: integer, b: bool) -> integer { " + body + " }";

    Lexer lexer(source);
    lexer.tokenize();
    Parser parser(lexer.get_tokens(), source);
    Program program = parser.parse();

    SemanticAnalyser analyser;
    analyser.analyse(program);
    return analyser.getErrors();
}

static bool accepts(const std::string &name, const std::string &body)
{
    std::vector<std::string> errors = analyseSource(body);
    bool passed = errors.empty();

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    for (const std::string &error : errors)
    {
        std::cout << "    unexpected error: " << error << "\n";
    }
    return passed;
}

// the analyser must report an error containing the given text
static bool rejects(const std::string &name, const std::string &body, const std::string &expectedError)
{
    std::vector<std::string> errors = analyseSource(body);
    bool passed = false;
    for (const std::string &error : errors)
    {
        if (error.find(expectedError) != std::string::npos)
        {
            passed = true;
        }
    }

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    if (!passed)
    {
        std::cout << "    expected error containing: " << expectedError << "\n";
        for (const std::string &error : errors)
        {
            std::cout << "    got: " << error << "\n";
        }
    }
    return passed;
}

bool runControlFlowSemanticTests()
{
    bool passing = true;

    // conditions
    passing &= accepts("if with comparison condition", "if n < 10 { n = 1; } return n;");
    passing &= accepts("if with bool variable", "if b { n = 1; } return n;");
    passing &= accepts("if with bool literal", "if true { n = 1; } return n;");
    passing &= accepts("while with comparison condition", "while n < 10 { n = n + 1; } return n;");
    passing &= accepts("else if chain", "if n < 1 { n = 1; } else if n < 2 { n = 2; } else { n = 3; } return n;");
    passing &= rejects("if with int condition", "if n { n = 1; } return n;",
                       "Condition of 'if' must be a boolean");
    passing &= rejects("while with int condition", "while 1 { n = 2; } return n;",
                       "Condition of 'while' must be a boolean");
    passing &= rejects("else if with int condition", "if b { n = 1; } else if n { n = 2; } return n;",
                       "Condition of 'if' must be a boolean");
    passing &= rejects("condition with undefined variable", "if missing { n = 1; } return n;",
                       "Undefined variable: 'missing'");

    // the condition error must not be double-reported when the condition itself is broken
    {
        std::vector<std::string> errors = analyseSource("if missing { } return n;");
        bool passed = errors.size() == 1;
        std::cout << (passed ? "PASSED: " : "FAILED: ") << "broken condition reports one error"
                  << (passed ? "" : "<------|") << "\n";
        passing &= passed;
    }

    // assignments
    passing &= accepts("assign matching type", "n = 5; return n;");
    passing &= accepts("assign to parameter", "b = n < 3; return n;");
    passing &= accepts("assign to local", "let x = 1; x = x + 1; return x;");
    passing &= rejects("assign wrong type", "n = true; return n;",
                       "Type mismatch in assignment to: 'n'");
    passing &= rejects("assign bool to int local", "let x = 1; x = n < 2; return x;",
                       "Type mismatch in assignment to: 'x'");
    passing &= rejects("assign to undefined variable", "y = 1; return n;",
                       "Undefined variable: 'y'");
    passing &= rejects("assign to function", "f = 1; return n;",
                       "Not a variable: 'f'");
    passing &= rejects("assignment value has an error", "n = missing; return n;",
                       "Undefined variable: 'missing'");

    // scoping
    passing &= rejects("variable declared in if block is out of scope after it",
                       "if b { let x = 1; } x = 2; return n;",
                       "Undefined variable: 'x'");
    passing &= rejects("variable declared in while block is out of scope after it",
                       "while b { let x = 1; } x = 2; return n;",
                       "Undefined variable: 'x'");
    passing &= accepts("outer variable visible inside blocks",
                       "let x = 0; while x < 3 { if b { x = x + 1; } } return x;");
    passing &= rejects("errors inside nested bodies are reported",
                       "while b { if b { n = true; } } return n;",
                       "Type mismatch in assignment to: 'n'");

    std::cout << "Control Flow Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
