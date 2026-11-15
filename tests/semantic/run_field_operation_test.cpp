#include "run_field_operation_test.h"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <iostream>
#include <string>
#include <vector>

// element-wise operators on fields

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

// exactly one error is reported, and it contains the given text
// (rejected expressions are written as statements, e.g. "a + i;", because
// "let x = a + i;" also reports that x's type cannot be inferred)
static bool rejects(const std::string &name, const std::string &source, const std::string &expectedError)
{
    std::vector<std::string> errors = analyseSource(source);
    bool passed = errors.size() == 1 && errors[0].find(expectedError) != std::string::npos;

    std::cout << (passed ? "PASSED: " : "FAILED: ") << name << (passed ? "" : "<------|") << "\n";
    if (!passed)
    {
        std::cout << "    expected one error containing: " << expectedError << "\n";
        for (const std::string &error : errors)
        {
            std::cout << "    got: " << error << "\n";
        }
    }
    return passed;
}

// statements inside a ruleset with one parameter of each kind
// declared let types (let c: field<f32, 4> = ...) check the type of each expression
static std::string ruleset(const std::string &statements)
{
    return "ruleset r(a: field<f32, 4>, b: field<f32, 4>, c: field<f32, 3>, "
           "i: field<i32, 4>, m: field<bool, 4>, s: f32, n: integer) -> field<f32, 4> { " +
           statements + " return a; }";
}

// the same statements inside a normal function
static std::string function(const std::string &statements)
{
    return "function f(a: field<f32, 4>, b: field<f32, 4>) -> integer { " +
           statements + " return 0; }";
}

bool runFieldOperationTests()
{
    bool passing = true;

    // arithmetic
    passing &= accepts("field + field", ruleset("let x: field<f32, 4> = a + b;"));
    passing &= accepts("field - * / field", ruleset("let x: field<f32, 4> = a - b * a / b;"));
    passing &= accepts("integer field arithmetic", ruleset("let x: field<i32, 4> = i * i - i;"));
    passing &= accepts("negate a field", ruleset("let x: field<f32, 4> = -a; let y: field<i32, 4> = -i;"));
    passing &= accepts("nested field expression", ruleset("let x: field<f32, 4> = (a + b) * (a - b);"));
    passing &= accepts("field result is returned",
                       "ruleset r(a: field<f32, 4>, b: field<f32, 4>) -> field<f32, 4> { return a * b; }");

    // broadcasting a uniform to every element
    passing &= accepts("field * uniform", ruleset("let x: field<f32, 4> = a * s;"));
    passing &= accepts("uniform * field", ruleset("let x: field<f32, 4> = 2.0 * a;"));
    passing &= accepts("mixed broadcast expression", ruleset("let x: field<f32, 4> = (a - 1.0) / s + b;"));
    passing &= accepts("integer field and uniform", ruleset("let x: field<i32, 4> = i * 2 + n;"));

    // comparisons give a bool field of the same length
    passing &= accepts("field < field", ruleset("let x: field<bool, 4> = a < b;"));
    passing &= accepts("field >= uniform", ruleset("let x: field<bool, 4> = a >= s;"));
    passing &= accepts("uniform == integer field", ruleset("let x: field<bool, 4> = 3 == i;"));
    passing &= accepts("bool field equality", ruleset("let x: field<bool, 4> = m == m; let y: field<bool, 4> = m != true;"));
    passing &= accepts("comparison of length 3 fields", ruleset("let x: field<bool, 3> = c <= c;"));

    // mismatched operands
    passing &= rejects("float field + integer field", ruleset("a + i;"),
                       "Binary expression has mismatched types");
    passing &= rejects("float field + integer uniform", ruleset("a + n;"),
                       "Binary expression has mismatched types");
    passing &= rejects("fields of different lengths", ruleset("a + c;"),
                       "Binary expression has mismatched types");
    passing &= rejects("comparison of different lengths", ruleset("a < c;"),
                       "Binary expression has mismatched types");
    passing &= rejects("arithmetic result has the field type", ruleset("let x: field<f32, 3> = a + b;"),
                       "Type mismatch for variable: 'x'");
    passing &= rejects("comparison result is a bool field", ruleset("let x: field<f32, 4> = a < b;"),
                       "Type mismatch for variable: 'x'");

    // operators each element type does not support
    passing &= rejects("integer field division", ruleset("i / i;"),
                       "Division of fields requires float elements");
    passing &= rejects("integer field divided by a uniform", ruleset("i / 2;"),
                       "Division of fields requires float elements");
    passing &= rejects("bool field arithmetic", ruleset("m + m;"),
                       "Arithmetic requires numeric types");
    passing &= rejects("bool field ordering", ruleset("m < m;"),
                       "Comparison requires numeric types");
    passing &= rejects("negate a bool field", ruleset("-m;"),
                       "Negation requires a numeric type");

    // field operations are only allowed in rulesets
    passing &= rejects("field arithmetic in a function", function("a + b;"),
                       "Field operations are only allowed in rulesets");
    passing &= rejects("field comparison in a function", function("a < b;"),
                       "Field operations are only allowed in rulesets");
    passing &= rejects("field broadcast in a function", function("a * 2.0;"),
                       "Field operations are only allowed in rulesets");
    passing &= rejects("field negation in a function", function("-a;"),
                       "Field operations are only allowed in rulesets");
    passing &= accepts("functions can still copy, pass and print fields",
                       function("let x = a; x = b; print(x); print([1.0, 2.0]);"));
    passing &= accepts("scalar arithmetic in a ruleset is unchanged", ruleset("let x: f32 = s * 2.0 - 1.0;"));

    // declared field types may only hold integer, float or bool
    passing &= rejects("field of fields parameter",
                       "function f(x: field<field<i32, 2>, 2>) -> integer { return 0; }",
                       "Field elements must be integer, float or bool (parameter 'x')");
    passing &= rejects("field of void return type",
                       "function f() -> field<void, 2> { return [1, 2]; }",
                       "Field elements must be integer, float or bool (return type of 'f')");
    passing &= rejects("field of fields ruleset parameter",
                       "ruleset r(x: field<field<f32, 1>, 1>) -> f32 { return 1.0; }",
                       "Field elements must be integer, float or bool (parameter 'x')");
    passing &= rejects("field of fields variable",
                       function("let x: field<field<f32, 1>, 1> = a;"),
                       "Field elements must be integer, float or bool (variable 'x')");

    std::cout << "Field Operation Semantic Test Result: " << (passing ? "PASSED" : "FAILED") << "\n";
    return passing;
}
