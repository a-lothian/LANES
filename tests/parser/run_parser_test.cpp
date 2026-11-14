#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "run_parser_test.h"

static Program parseSource(const std::string &source)
{
    Lexer lexer(source);

    lexer.tokenize();

    Parser parser(
        lexer.get_tokens(),
        source);

    return parser.parse();
}

static bool expect(bool condition, const std::string &name)
{
    std::cout
        << (condition ? "PASSED: " : "FAILED: ")
        << name
        << "\n";

    return condition;
}

static bool rejectsSource(const std::string &source)
{
    try
    {
        parseSource(source);
    }
    catch (const ParseError &)
    {
        return true;
    }

    return false;
}

// statements of the first function in a parsed program
static std::vector<std::unique_ptr<Statement>> &functionBody(Program &program)
{
    auto *function = dynamic_cast<FunctionDecl *>(program.declarations[0].get());
    return function->body->statements;
}

bool run_parser_tests()
{
    bool passing = true;

    // TEST CASE 1
    // empty source file should produce an empty Program ast
    {
        Program program = parseSource("");

        passing &= expect(
            program.declarations.empty(),
            "parser_empty_program");
    }

    // TEST CASE 2
    // parsing for basic function declaration with no parameters (and no body)
    {
        Program program = parseSource("function main() -> integer {}");
        auto *function = dynamic_cast<FunctionDecl *>(program.declarations[0].get());

        // checking that the function has the correct name + parameters + return type (and body)
        bool valid =
            program.declarations.size() == 1 &&
            function != nullptr &&
            function->name == "main" &&
            function->parameters.empty() &&
            function->returnType.kind == TypeKind::Int &&
            function->body != nullptr;
        passing &= expect(valid, "parser_basic_function");
    }

    // TEST CASE 3
    // parsing for "multiple-typed" function parameters
    {
        Program program = parseSource("function add(x: integer, y: f32) -> f32 {}");

        auto *function = dynamic_cast<FunctionDecl *>(program.declarations[0].get());

        // checking that the function has the correct parameter types
        bool valid =
            function != nullptr &&
            function->parameters.size() == 2 &&
            function->parameters[0].name == "x" &&
            function->parameters[0].type.kind ==
                TypeKind::Int &&
            function->parameters[1].name == "y" &&
            function->parameters[1].type.kind ==
                TypeKind::Float;

        passing &= expect(valid, "parser_function_parameters");
    }

    // TEST CASE 4
    // parsing for "LANES-specific" field<T, length> type in a Ruleset
    {
        Program program = parseSource("ruleset scale(x: field<f32, 4>, k: f32) "
                                      "-> field<f32, 4> {}");
        auto *ruleset = dynamic_cast<RulesetDecl *>(program.declarations[0].get());

        // checking that the ruleset has the correct parameter and return types
        bool valid =
            ruleset != nullptr &&
            ruleset->parameters.size() == 2 &&
            ruleset->parameters[0].type.kind ==
                TypeKind::Field &&
            ruleset->parameters[0].type.elementType !=
                nullptr &&
            ruleset->parameters[0].type.elementType->kind ==
                TypeKind::Float &&
            ruleset->parameters[0].type.length == 4 &&
            ruleset->returnType.length == 4 &&
            ruleset->returnType.kind ==
                TypeKind::Field &&
            ruleset->returnType.elementType !=
                nullptr &&
            ruleset->returnType.elementType->kind ==
                TypeKind::Float;

        passing &= expect(valid, "parser_ruleset_field_type");
    }

    {
        bool rejected = false;

        try
        {
            parseSource("let x = 1;");
        }
        catch (const ParseError &)
        {
            rejected = true;
        }

        passing &= expect(
            rejected,
            "parser_rejects_invalid_top_level");
    }

    {
        Program program = parseSource(
            "function main() -> integer { "
            "let x = 12; "
            "return x; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *letStatement =
            function == nullptr ||
                    function->body->statements.empty()
                ? nullptr
                : dynamic_cast<LetStatement *>(
                      function->body->statements[0].get());

        auto *returnStatement =
            function == nullptr ||
                    function->body->statements.size() < 2
                ? nullptr
                : dynamic_cast<ReturnStatement *>(
                      function->body->statements[1].get());

        auto *initializer =
            letStatement == nullptr
                ? nullptr
                : dynamic_cast<LiteralExpr *>(
                      letStatement->initializer.get());

        auto *returnValue =
            returnStatement == nullptr
                ? nullptr
                : dynamic_cast<VariableExpr *>(
                      returnStatement->value.get());

        bool valid =
            function != nullptr &&
            function->body->statements.size() == 2 &&
            letStatement != nullptr &&
            letStatement->name == "x" &&
            letStatement->type.kind == TypeKind::Void &&
            initializer != nullptr &&
            initializer->value == "12" &&
            initializer->type.kind == TypeKind::Int &&
            returnStatement != nullptr &&
            returnValue != nullptr &&
            returnValue->name == "x";

        passing &= expect(
            valid,
            "parser_let_and_return_statements");
    }

    {
        Program program = parseSource(
            "function main() -> f32 { "
            "let x: f32 = 5.25; "
            "return x; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *letStatement =
            function == nullptr ||
                    function->body->statements.empty()
                ? nullptr
                : dynamic_cast<LetStatement *>(
                      function->body->statements[0].get());

        auto *initializer =
            letStatement == nullptr
                ? nullptr
                : dynamic_cast<LiteralExpr *>(
                      letStatement->initializer.get());

        bool valid =
            letStatement != nullptr &&
            letStatement->type.kind == TypeKind::Float &&
            initializer != nullptr &&
            initializer->type.kind == TypeKind::Float &&
            initializer->value == "5.25";

        passing &= expect(
            valid,
            "parser_typed_let_statement");
    }

    {
        Program program = parseSource(
            "function test() -> integer { "
            "{ 7; } "
            "return 1; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *nestedBlock =
            function == nullptr ||
                    function->body->statements.empty()
                ? nullptr
                : dynamic_cast<BlockStatement *>(
                      function->body->statements[0].get());

        auto *expressionStatement =
            nestedBlock == nullptr ||
                    nestedBlock->statements.empty()
                ? nullptr
                : dynamic_cast<ExpressionStatement *>(
                      nestedBlock->statements[0].get());

        bool valid =
            nestedBlock != nullptr &&
            expressionStatement != nullptr;

        passing &= expect(
            valid,
            "parser_nested_block_and_expression_statement");
    }

    {
        bool rejected = false;

        try
        {
            parseSource(
                "function main() -> integer { "
                "let x = 1 "
                "return x; "
                "}");
        }
        catch (const ParseError &)
        {
            rejected = true;
        }

        passing &= expect(
            rejected,
            "parser_rejects_missing_semicolon");
    }

    {
        Program program = parseSource(
            "function calc(x: integer, y: integer) -> integer { "
            "let result = x + y * 2; "
            "return result; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *letStatement =
            dynamic_cast<LetStatement *>(
                function->body->statements[0].get());

        auto *add =
            dynamic_cast<BinaryExpr *>(
                letStatement->initializer.get());

        auto *multiply =
            add == nullptr
                ? nullptr
                : dynamic_cast<BinaryExpr *>(
                      add->right.get());

        bool valid =
            add != nullptr &&
            add->op == BinaryOperator::Add &&
            multiply != nullptr &&
            multiply->op == BinaryOperator::Multiply;

        passing &= expect(
            valid,
            "parser_operator_precedence");
    }

    // TEST CASE 10 (parentheses override precedence)
    {
        Program program = parseSource(
            "function calc(x: integer, y: integer) -> integer { "
            "return (x + y) * 2; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *returnStatement =
            dynamic_cast<ReturnStatement *>(
                function->body->statements[0].get());

        auto *multiply =
            dynamic_cast<BinaryExpr *>(
                returnStatement->value.get());

        auto *add =
            multiply == nullptr
                ? nullptr
                : dynamic_cast<BinaryExpr *>(
                      multiply->left.get());

        bool valid =
            multiply != nullptr &&
            multiply->op == BinaryOperator::Multiply &&
            add != nullptr &&
            add->op == BinaryOperator::Add;

        passing &= expect(
            valid,
            "parser_parenthesised_expression");
    }

    // TEST CASE 11 (comparisons)
    {
        Program program = parseSource(
            "function compare(x: integer, y: integer) -> bool { "
            "return x + 1 >= y * 2; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *returnStatement =
            dynamic_cast<ReturnStatement *>(
                function->body->statements[0].get());

        auto *comparison =
            dynamic_cast<BinaryExpr *>(
                returnStatement->value.get());

        auto *left =
            comparison == nullptr
                ? nullptr
                : dynamic_cast<BinaryExpr *>(
                      comparison->left.get());

        auto *right =
            comparison == nullptr
                ? nullptr
                : dynamic_cast<BinaryExpr *>(
                      comparison->right.get());

        bool valid =
            comparison != nullptr &&
            comparison->op ==
                BinaryOperator::GreaterThanEqual &&
            left != nullptr &&
            left->op == BinaryOperator::Add &&
            right != nullptr &&
            right->op == BinaryOperator::Multiply;

        passing &= expect(
            valid,
            "parser_comparison_expression");
    }

    // TEST CASE 12 (equality)
    {
        Program program = parseSource(
            "function equal(x: integer, y: integer) -> bool { "
            "return x == y; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *returnStatement =
            dynamic_cast<ReturnStatement *>(
                function->body->statements[0].get());

        auto *equality =
            dynamic_cast<BinaryExpr *>(
                returnStatement->value.get());

        bool valid =
            equality != nullptr &&
            equality->op == BinaryOperator::Equal;

        passing &= expect(
            valid,
            "parser_equality_expression");
    }

    // TEST CASE 13 (unary minus)
    {
        Program program = parseSource(
            "function negate(x: integer) -> integer { "
            "return -x * 2; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *returnStatement =
            dynamic_cast<ReturnStatement *>(
                function->body->statements[0].get());

        auto *multiply =
            dynamic_cast<BinaryExpr *>(
                returnStatement->value.get());

        auto *negate =
            multiply == nullptr
                ? nullptr
                : dynamic_cast<UnaryExpr *>(
                      multiply->left.get());

        bool valid =
            multiply != nullptr &&
            multiply->op == BinaryOperator::Multiply &&
            negate != nullptr &&
            negate->op == UnaryOperator::Negate;

        passing &= expect(
            valid,
            "parser_unary_expression");
    }

    // TEST CASE 14 (boolean literal)
    {
        Program program = parseSource(
            "function flag() -> bool { "
            "return true; "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *returnStatement =
            dynamic_cast<ReturnStatement *>(
                function->body->statements[0].get());

        auto *literal =
            dynamic_cast<LiteralExpr *>(
                returnStatement->value.get());

        bool valid =
            literal != nullptr &&
            literal->value == "true" &&
            literal->type.kind == TypeKind::Bool;

        passing &= expect(
            valid,
            "parser_boolean_literal");
    }

    // TEST CASE 15 (function calls)
    {
        Program program = parseSource(
            "function main() -> integer { "
            "return add(1, 2 * 3); "
            "}");

        auto *function =
            dynamic_cast<FunctionDecl *>(
                program.declarations[0].get());

        auto *returnStatement =
            dynamic_cast<ReturnStatement *>(
                function->body->statements[0].get());

        auto *call =
            dynamic_cast<CallExpr *>(
                returnStatement->value.get());

        auto *secondArgument =
            call == nullptr ||
                    call->arguments.size() < 2
                ? nullptr
                : dynamic_cast<BinaryExpr *>(
                      call->arguments[1].get());

        bool valid =
            call != nullptr &&
            call->name == "add" &&
            call->arguments.size() == 2 &&
            secondArgument != nullptr &&
            secondArgument->op ==
                BinaryOperator::Multiply;

        passing &= expect(
            valid,
            "parser_function_call");
    }

    // TEST CASE 16 (representative complete LANES program)
    {
        Program program = parseSource(
            "function add(x: integer, y: integer) -> integer { "
            "return x + y; "
            "} "
            "ruleset scale(values: field<f32, 8>, factor: f32) -> field<f32, 8> { "
            "return values * factor; "
            "} "
            "function main() -> integer { "
            "let result: integer = add(4, 5); "
            "return result; "
            "}");

        auto *add = dynamic_cast<FunctionDecl *>(
            program.declarations[0].get());
        auto *scale = dynamic_cast<RulesetDecl *>(
            program.declarations[1].get());
        auto *mainFunction = dynamic_cast<FunctionDecl *>(
            program.declarations[2].get());

        bool valid =
            program.declarations.size() == 3 &&
            add != nullptr &&
            add->name == "add" &&
            scale != nullptr &&
            scale->name == "scale" &&
            mainFunction != nullptr &&
            mainFunction->name == "main" &&
            mainFunction->body->statements.size() == 2;

        passing &= expect(
            valid,
            "parser_complete_program");
    }

    // TEST CASE 17 (bare return statement)
    {
        Program program = parseSource(
            "function stop() -> void { return; }");

        auto *function = dynamic_cast<FunctionDecl *>(
            program.declarations[0].get());
        auto *returnStatement = dynamic_cast<ReturnStatement *>(
            function->body->statements[0].get());

        bool valid =
            returnStatement != nullptr &&
            returnStatement->value == nullptr;

        passing &= expect(
            valid,
            "parser_empty_return");
    }

    // TEST CASE 18 (left associativity)
    {
        Program program = parseSource(
            "function calc() -> integer { return 10 - 3 - 2; }");

        auto *function = dynamic_cast<FunctionDecl *>(
            program.declarations[0].get());
        auto *returnStatement = dynamic_cast<ReturnStatement *>(
            function->body->statements[0].get());
        auto *outerSubtract = dynamic_cast<BinaryExpr *>(
            returnStatement->value.get());
        auto *innerSubtract =
            outerSubtract == nullptr
                ? nullptr
                : dynamic_cast<BinaryExpr *>(
                      outerSubtract->left.get());

        bool valid =
            outerSubtract != nullptr &&
            outerSubtract->op == BinaryOperator::Subtract &&
            innerSubtract != nullptr &&
            innerSubtract->op == BinaryOperator::Subtract;

        passing &= expect(
            valid,
            "parser_left_associativity");
    }

    // TEST CASE 19 (inequality operator)
    {
        Program program = parseSource(
            "function compare(x: integer, y: integer) -> bool { "
            "return x != y; "
            "}");

        auto *function = dynamic_cast<FunctionDecl *>(
            program.declarations[0].get());
        auto *returnStatement = dynamic_cast<ReturnStatement *>(
            function->body->statements[0].get());
        auto *inequality = dynamic_cast<BinaryExpr *>(
            returnStatement->value.get());

        bool valid =
            inequality != nullptr &&
            inequality->op == BinaryOperator::NotEqual;

        passing &= expect(
            valid,
            "parser_inequality_expression");
    }

    // TEST CASE 20 (nested and zero-argument function calls)
    {
        Program program = parseSource(
            "function main() -> integer { "
            "return outer(inner(1), ping()); "
            "}");

        auto *function = dynamic_cast<FunctionDecl *>(
            program.declarations[0].get());
        auto *returnStatement = dynamic_cast<ReturnStatement *>(
            function->body->statements[0].get());
        auto *outer = dynamic_cast<CallExpr *>(
            returnStatement->value.get());
        auto *inner =
            outer == nullptr || outer->arguments.empty()
                ? nullptr
                : dynamic_cast<CallExpr *>(
                      outer->arguments[0].get());
        auto *ping =
            outer == nullptr || outer->arguments.size() < 2
                ? nullptr
                : dynamic_cast<CallExpr *>(
                      outer->arguments[1].get());

        bool valid =
            outer != nullptr &&
            outer->name == "outer" &&
            outer->arguments.size() == 2 &&
            inner != nullptr &&
            inner->name == "inner" &&
            inner->arguments.size() == 1 &&
            ping != nullptr &&
            ping->name == "ping" &&
            ping->arguments.empty();

        passing &= expect(
            valid,
            "parser_nested_function_calls");
    }

    // TEST CASE 21-27 (malformed syntax rejection)
    passing &= expect(
        rejectsSource("function main() -> mystery {}"),
        "parser_rejects_unknown_type");

    passing &= expect(
        rejectsSource("function main(x integer) -> integer {}"),
        "parser_rejects_malformed_parameter");

    passing &= expect(
        rejectsSource("function main() -> integer { return 1; "),
        "parser_rejects_unclosed_block");

    passing &= expect(
        rejectsSource("function main() -> integer { return 1 +; }"),
        "parser_rejects_missing_operand");

    passing &= expect(
        rejectsSource("function main() -> integer { return add(1,); }"),
        "parser_rejects_trailing_call_comma");

    passing &= expect(
        rejectsSource("function main() -> integer { return @; }"),
        "parser_rejects_invalid_token");

    // ParseError should identify both the unexpected token and source location
    {
        bool validDiagnostic = false;

        try
        {
            parseSource(
                "function main() -> integer {\n"
                "let x 5;\n"
                "}");
        }
        catch (const ParseError &error)
        {
            std::string message = error.what();

            validDiagnostic =
                message.find("Expected '=' in variable declaration.") !=
                    std::string::npos &&
                message.find("Found '5'") != std::string::npos &&
                message.find("line 2, column 7") != std::string::npos;
        }

        passing &= expect(
            validDiagnostic,
            "parser_error_diagnostic_location");
    }

    // field lengths: non-multiples of 4, nesting (nested fields are rejected later, in semantic analysis)
    {
        Program program = parseSource(
            "function main(x: field<i32, 7>, y: field<field<bool, 2>, 3>) -> integer { "
            "return 0; "
            "}");

        auto *function = dynamic_cast<FunctionDecl *>(program.declarations[0].get());
        const Type *x = function == nullptr ? nullptr : &function->parameters[0].type;
        const Type *y = function == nullptr ? nullptr : &function->parameters[1].type;

        bool valid =
            x != nullptr &&
            x->kind == TypeKind::Field &&
            x->length == 7 &&
            x->elementType->kind == TypeKind::Int &&
            y != nullptr &&
            y->length == 3 &&
            y->elementType->kind == TypeKind::Field &&
            y->elementType->length == 2;

        passing &= expect(valid, "parser_field_length");
    }

    // largest length that fits in 32 bits is accepted (memory limits are checked later)
    {
        Program program = parseSource(
            "function f(x: field<f32, 4294967295>) -> integer { return 0; }");
        auto *function = dynamic_cast<FunctionDecl *>(program.declarations[0].get());

        passing &= expect(
            function != nullptr &&
                function->parameters[0].type.length == 4294967295u,
            "parser_field_length_max");
    }

    // a field length must be a positive integer literal
    {
        bool rejected =
            rejectsSource("function f(x: field<f32>) -> integer { return 0; }") &&
            rejectsSource("function f(x: field<f32, 0>) -> integer { return 0; }") &&
            rejectsSource("function f(x: field<f32, 4.0>) -> integer { return 0; }") &&
            rejectsSource("function f(x: field<f32, -4>) -> integer { return 0; }") &&
            rejectsSource("function f(x: field<f32, n>) -> integer { return 0; }") &&
            rejectsSource("function f(x: field<f32, 4294967296>) -> integer { return 0; }") &&
            rejectsSource("function f(x: field<f32, 99999999999999999999999>) -> integer { return 0; }") &&
            rejectsSource("function f(x: field<f32, 4) -> integer { return 0; }");

        passing &= expect(rejected, "parser_rejects_invalid_field_length");
    }

    // the error names the problem and where it is
    {
        bool validDiagnostic = false;

        try
        {
            parseSource("function f(x: field<f32, 0>) -> integer { return 0; }");
        }
        catch (const ParseError &error)
        {
            std::string message = error.what();
            validDiagnostic =
                message.find("Field length must be at least 1.") != std::string::npos &&
                message.find("line 1, column 26") != std::string::npos;
        }

        passing &= expect(validDiagnostic, "parser_field_length_diagnostic");
    }

    // TEST CASES 28-34 (control flow and assignment)
    {
        // if without else
        Program program = parseSource(
            "function f() -> integer { if a < b { return 1; } return 2; }");
        auto &statements = functionBody(program);

        auto *ifStatement = dynamic_cast<IfStatement *>(statements[0].get());
        bool valid =
            statements.size() == 2 &&
            ifStatement != nullptr &&
            dynamic_cast<BinaryExpr *>(ifStatement->condition.get()) != nullptr &&
            ifStatement->thenBlock->statements.size() == 1 &&
            ifStatement->elseBranch == nullptr;

        passing &= expect(valid, "parser_if_statement");
    }

    {
        // if with else
        Program program = parseSource(
            "function f() -> integer { if a { return 1; } else { return 2; } }");
        auto &statements = functionBody(program);

        auto *ifStatement = dynamic_cast<IfStatement *>(statements[0].get());
        bool valid =
            ifStatement != nullptr &&
            dynamic_cast<VariableExpr *>(ifStatement->condition.get()) != nullptr &&
            dynamic_cast<BlockStatement *>(ifStatement->elseBranch.get()) != nullptr;

        passing &= expect(valid, "parser_if_else_statement");
    }

    {
        // else if nests another IfStatement inside the else branch
        Program program = parseSource(
            "function f() -> integer { "
            "if a { return 1; } else if b { return 2; } else { return 3; } }");
        auto &statements = functionBody(program);

        auto *outer = dynamic_cast<IfStatement *>(statements[0].get());
        auto *inner = outer != nullptr
                          ? dynamic_cast<IfStatement *>(outer->elseBranch.get())
                          : nullptr;
        bool valid =
            inner != nullptr &&
            dynamic_cast<BlockStatement *>(inner->elseBranch.get()) != nullptr;

        passing &= expect(valid, "parser_else_if_chain");
    }

    {
        // while loop containing an assignment
        Program program = parseSource(
            "function f() -> integer { let i = 0; while i < 10 { i = i + 1; } return i; }");
        auto &statements = functionBody(program);

        auto *loop = dynamic_cast<WhileStatement *>(statements[1].get());
        auto *assignment = loop != nullptr && loop->body->statements.size() == 1
                               ? dynamic_cast<AssignmentStatement *>(loop->body->statements[0].get())
                               : nullptr;
        bool valid =
            loop != nullptr &&
            dynamic_cast<BinaryExpr *>(loop->condition.get()) != nullptr &&
            assignment != nullptr &&
            assignment->name == "i" &&
            dynamic_cast<BinaryExpr *>(assignment->value.get()) != nullptr;

        passing &= expect(valid, "parser_while_and_assignment");
    }

    {
        // "x = ..." is an assignment but "x == ..." and "x(...)" stay expression statements
        Program program = parseSource(
            "function f() -> integer { let x = 1; x = 2; x == 2; g(x); return x; }");
        auto &statements = functionBody(program);

        bool valid =
            statements.size() == 5 &&
            dynamic_cast<AssignmentStatement *>(statements[1].get()) != nullptr &&
            dynamic_cast<ExpressionStatement *>(statements[2].get()) != nullptr &&
            dynamic_cast<ExpressionStatement *>(statements[3].get()) != nullptr;

        passing &= expect(valid, "parser_assignment_vs_expression_statement");
    }

    {
        // brackets around a condition are just grouping, and statements nest
        Program program = parseSource(
            "function f() -> integer { while (a < b) { if (a == 1) { a = 2; } } return a; }");
        auto &statements = functionBody(program);

        auto *loop = dynamic_cast<WhileStatement *>(statements[0].get());
        bool valid =
            loop != nullptr &&
            loop->body->statements.size() == 1 &&
            dynamic_cast<IfStatement *>(loop->body->statements[0].get()) != nullptr;

        passing &= expect(valid, "parser_bracketed_condition_and_nesting");
    }

    {
        // malformed control flow is rejected
        bool rejected =
            rejectsSource("function f() -> integer { if a return 1; }") &&
            rejectsSource("function f() -> integer { if { return 1; } }") &&
            rejectsSource("function f() -> integer { while { } }") &&
            rejectsSource("function f() -> integer { else { } }") &&
            rejectsSource("function f() -> integer { if a { } else return 1; }") &&
            rejectsSource("function f() -> integer { x = ; }") &&
            rejectsSource("function f() -> integer { x = 1 }");

        passing &= expect(rejected, "parser_rejects_malformed_control_flow");
    }

    std::cout
        << "Parser Test Result: "
        << (passing ? "PASSED" : "FAILED")
        << "\n";

    return passing;
}