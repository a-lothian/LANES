#include "run_symbol_table_test.h"
#include "symbol_table.hpp"

#include <iostream>

// helper to create variable symbols
static Symbol makeVariable(TypeKind type)
{
    return Symbol{
        SymbolKind::Variable,
        Type{type, nullptr},
        {}};
}

// test variable declarations and duplicates
static bool testDeclarations()
{
    SymbolTable table;
    Symbol x = makeVariable(TypeKind::Int);

    bool first = table.declare("x", x);
    bool duplicate = table.declare("x", x);

    return first && !duplicate;
}

// test looking up declared and undeclared symbols
static bool testLookup()
{
    SymbolTable table;

    table.declare("x", makeVariable(TypeKind::Int));

    return table.lookup("x") != nullptr &&
           table.lookup("y") == nullptr;
}

// test entering and leaving scopes
static bool testScopes()
{
    SymbolTable table;

    table.declare("x", makeVariable(TypeKind::Int));

    table.enterScope();
    table.declare("y", makeVariable(TypeKind::Float));

    bool inside = table.lookup("x") != nullptr &&
                  table.lookup("y") != nullptr;

    table.exitScope();

    bool outside = table.lookup("x") != nullptr &&
                   table.lookup("y") == nullptr;

    return inside && outside;
}

// test variable shadowing
static bool testShadowing()
{
    SymbolTable table;

    table.declare("x", makeVariable(TypeKind::Int));

    table.enterScope();
    table.declare("x", makeVariable(TypeKind::Float));

    const Symbol *local = table.lookup("x");

    bool localIsFloat = local != nullptr &&
                        local->type.kind == TypeKind::Float;

    table.exitScope();

    const Symbol *global = table.lookup("x");

    bool globalIsInt = global != nullptr &&
                       global->type.kind == TypeKind::Int;

    return localIsFloat && globalIsInt;
}

// test looking up symbols only in the current scope
static bool testCurrentScope()
{
    SymbolTable table;

    table.declare("x", makeVariable(TypeKind::Int));

    table.enterScope();

    bool outerVisible = table.lookup("x") != nullptr;
    bool notLocal = table.lookupCurrentScope("x") == nullptr;

    table.declare("y", makeVariable(TypeKind::Float));

    bool localVisible = table.lookupCurrentScope("y") != nullptr;

    table.exitScope();

    bool globalVisible = table.lookupCurrentScope("x") != nullptr;

    return outerVisible && notLocal &&
           localVisible && globalVisible;
}

// run all symbol table tests
bool runSymbolTableTests()
{
    bool allPassed = true;

    struct TestCase
    {
        const char *name;
        bool (*test)();
    };

    const TestCase tests[] = {
        {"symbol_declarations", testDeclarations},
        {"symbol_lookup", testLookup},
        {"symbol_scopes", testScopes},
        {"symbol_shadowing", testShadowing},
        {"symbol_current_scope", testCurrentScope}};

    for (const auto &test : tests)
    {
        bool passed = test.test();

        std::cout << (passed ? "PASSED: " : "FAILED: ")
                  << test.name << "\n";

        if (!passed)
        {
            allPassed = false;
        }
    }

    std::cout << "Symbol Table Test Result: "
              << (allPassed ? "PASSED" : "FAILED")
              << "\n";

    return allPassed;
}
