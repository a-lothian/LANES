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
    // parsing for "LANES-specific" field<T> type in a Ruleset
    {
        Program program = parseSource("ruleset scale(x: field<f32>, k: f32) "
                                      "-> field<f32> {}");
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
            ruleset->returnType.kind ==
                TypeKind::Field &&
            ruleset->returnType.elementType !=
                nullptr &&
            ruleset->returnType.elementType->kind ==
                TypeKind::Float;

        passing &= expect(valid, "parser_ruleset_field_type");
    }

    // TEST CASE 5
    // statement not legal at the top level
    {
        bool rejected = false; // should throw a ParseError (just for testing purposes, we don't really care about the error message)

        try
        {
            parseSource("let x = 1;");
        }
        catch (const ParseError &)
        {
            rejected = true;
        }

        passing &= expect(rejected, "parser_rejects_invalid_top_level");
    }

    // printing the overall result of the parser tests
    std::cout
        << "Parser Test Result: "
        << (passing ? "PASSED" : "FAILED")
        << "\n";

    return passing;
}