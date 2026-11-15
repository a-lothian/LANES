#include "run_ruleset_contract_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// a ruleset is one fused loop over its fields, so its body has a fixed shape

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

// exactly the given errors are reported, in order
static bool rejects(const std::string &name, const std::string &source,
                    const std::vector<std::string> &expected)
{
    std::vector<std::string> errors = analyseSource(source);
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

static bool rejects(const std::string &name, const std::string &source, const std::string &expected)
{
    return rejects(name, source, std::vector<std::string>{expected});
}

// a ruleset with a field and a uniform parameter
static std::string ruleset(const std::string &result, const std::string &body)
{
    return "ruleset r(x: field<f32, 4>, s: f32) -> " + result + " { " + body + " }";
}

static const std::string TOTAL = "ruleset total(x: field<f32, 4>) -> f32 { return sum(x); } ";

bool runRulesetContractTests()
{
    bool passing = true;

    // what a ruleset can be
    passing &= accepts("field result from element-wise operations",
                       ruleset("field<f32, 4>", "let y = x * s; y = abs(y); return select(y > 1.0, y, 0.0);"));
    passing &= accepts("uniform result from a reduction",
                       ruleset("f32", "let t = sum(x * s); let m = max_of(x); return t - m;"));
    passing &= accepts("uniform result from a ruleset that reduces",
                       TOTAL + ruleset("f32", "return total(x) * 2.0;"));
    passing &= accepts("field result using a uniform from another ruleset",
                       TOTAL + ruleset("field<f32, 4>", "return x / total(x);"));
    passing &= accepts("length is not a reduction, so a field result can use it",
                       ruleset("field<f32, 4>", "return x * to_f32(length(x));"));

    // straight-line body
    passing &= rejects("if statement",
                       ruleset("field<f32, 4>", "if s > 0.0 { return x; } return x * s;"),
                       "Ruleset 'r' cannot contain if statements (use select instead)");
    passing &= rejects("while loop",
                       ruleset("field<f32, 4>", "while s > 0.0 { s = s - 1.0; } return x;"),
                       "Ruleset 'r' cannot contain while loops");
    passing &= rejects("nested block",
                       ruleset("field<f32, 4>", "{ let y = x; } return x;"),
                       "Ruleset 'r' cannot contain nested blocks");
    passing &= rejects("return before the end",
                       ruleset("field<f32, 4>", "return x; let y = x;"),
                       "Ruleset 'r' must have a single return, as its last statement");
    passing &= rejects("void ruleset",
                       "ruleset r(x: field<f32, 4>) -> void { let y = x; }",
                       "Ruleset 'r' must return a value (rulesets have no other effects)");

    // calls
    passing &= rejects("print in a ruleset",
                       ruleset("field<f32, 4>", "print(s); return x;"),
                       "Ruleset 'r' cannot call print (rulesets have no I/O)");
    passing &= rejects("function call in a ruleset",
                       "function half(n: f32) -> f32 { return n / 2.0; } " +
                           ruleset("field<f32, 4>", "return x * half(s);"),
                       "Ruleset 'r' cannot call function 'half' (rulesets may only call rulesets and built-ins)");

    // reductions and result kinds
    passing &= rejects("field result with a reduction",
                       ruleset("field<f32, 4>", "return x / sum(x);"),
                       "Ruleset 'r' returns a field, so it cannot use reductions "
                       "(move the reduction into its own ruleset)");
    passing &= rejects("uniform result without a reduction",
                       ruleset("f32", "return s * 2.0;"),
                       "Ruleset 'r' returns a uniform, so it must use a reduction "
                       "(sum, min_of, max_of, any or all)");
    passing &= rejects("uniform result from length only",
                       ruleset("integer", "return length(x);"),
                       "Ruleset 'r' returns a uniform, so it must use a reduction "
                       "(sum, min_of, max_of, any or all)");

    // recursion: with no if statements a recursive ruleset could never stop
    passing &= rejects("ruleset calls itself",
                       "ruleset r(x: field<f32, 4>) -> field<f32, 4> { return r(x); }",
                       "Recursive ruleset: 'r' (rulesets cannot call themselves, directly or through other rulesets)");
    passing &= rejects("rulesets call each other",
                       "ruleset a(x: field<f32, 4>) -> field<f32, 4> { return b(x); } "
                       "ruleset b(x: field<f32, 4>) -> field<f32, 4> { return a(x); }",
                       std::vector<std::string>{
                           "Recursive ruleset: 'a' (rulesets cannot call themselves, directly or through other rulesets)",
                        "Recursive ruleset: 'b' (rulesets cannot call themselves, directly or through other rulesets)"});

    // functions are not rulesets: none of this applies to them
    passing &= accepts("functions can branch, print and return uniforms",
                       "function f(n: integer) -> integer { if n > 0 { print(n); } return n * 2; }");

    std::cout << "Ruleset Contract Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
