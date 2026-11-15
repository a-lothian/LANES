#include "run_recursion_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// fields have fixed addresses, so recursive functions (direct or mutual) may not use them

static std::vector<std::string> analyseSource(const std::string &source)
{
    Lexer lexer(source);
    lexer.tokenize();
    Parser parser(lexer.get_tokens(), source);
    Program program = parser.parse();

    SemanticAnalyser analyser;
    analyser.analyse(program);
    return analyser.getErrors();
}

static bool accepts(const std::string &name, const std::string &source)
{
    std::vector<std::string> errors = analyseSource(source);
    bool passed = errors.empty();

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    for (const std::string &error : errors)
    {
        std::cout << "    unexpected error: " << error << "\n";
    }
    return passed;
}

// exactly the listed functions are reported as recursive functions using fields
static bool rejects(const std::string &name, const std::string &source,
                    const std::vector<std::string> &functions)
{
    std::vector<std::string> errors = analyseSource(source);

    std::vector<std::string> expected;
    for (const std::string &function : functions)
    {
        expected.push_back(
            "Recursive function cannot use fields: '" + function + "'");
    }

    bool passed = errors == expected;

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    if (!passed)
    {
        for (const std::string &error : expected)
        {
            std::cout << "    expected: " << error << "\n";
        }
        for (const std::string &error : errors)
        {
            std::cout << "    got: " << error << "\n";
        }
    }
    return passed;
}

bool runRecursionTests()
{
    bool passing = true;

    passing &= accepts(
        "recursion without fields",
        "function fact(n: integer) -> integer { "
        "  if n <= 1 { return 1; } "
        "  return n * fact(n - 1); "
        "}");

    passing &= accepts(
        "fields without recursion",
        "function make() -> field<integer, 2> { return [1, 2]; } "
        "function main() -> integer { let a = make(); let b = make(); return 0; }");

    // count is not running twice at once while make runs, so make's blocks are safe
    passing &= accepts(
        "recursive function calls a function that uses fields",
        "function make() -> field<integer, 2> { return [1, 2]; } "
        "function size() -> integer { let a = make(); return 2; } "
        "function count(n: integer) -> integer { "
        "  if n == 0 { return size(); } "
        "  return count(n - 1); "
        "}");

    passing &= rejects(
        "field local in recursive function",
        "function f(n: integer) -> integer { "
        "  let a = [n, n]; "
        "  if n == 0 { return 0; } "
        "  return f(n - 1); "
        "}",
        {"f"});

    passing &= rejects(
        "field parameter in recursive function",
        "function f(a: field<f32, 4>, n: integer) -> integer { "
        "  if n == 0 { return 0; } "
        "  return f(a, n - 1); "
        "}",
        {"f"});

    passing &= rejects(
        "field return in recursive function",
        "function f(n: integer) -> field<integer, 1> { "
        "  if n == 0 { return [0]; } "
        "  return f(n - 1); "
        "}",
        {"f"});

    passing &= rejects(
        "unused field literal in recursive function",
        "function f(n: integer) -> integer { "
        "  [1, 2]; "
        "  if n == 0 { return 0; } "
        "  return f(n - 1); "
        "}",
        {"f"});

    passing &= rejects(
        "recursion inside a loop body",
        "function f(n: integer) -> integer { "
        "  while n > 0 { let a = [n]; n = f(n - 1); } "
        "  return 0; "
        "}",
        {"f"});

    // ping and pong are both recursive through each other, only pong uses fields
    passing &= rejects(
        "mutual recursion",
        "function ping(n: integer) -> integer { "
        "  if n == 0 { return 0; } "
        "  return pong(n - 1); "
        "} "
        "function pong(n: integer) -> integer { "
        "  let a = [n]; "
        "  return ping(n); "
        "}",
        {"pong"});

    passing &= rejects(
        "longer cycle",
        "function a(n: integer) -> integer { let x = [n]; return b(n); } "
        "function b(n: integer) -> integer { return c(n); } "
        "function c(n: integer) -> integer { let y = [true]; return a(n); }",
        {"a", "c"});

    std::cout << "Recursion Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
