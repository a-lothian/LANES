#pragma once

// runs the semantic tests for the ruleset contract (straight-line body, no I/O or
// function calls, reductions only for uniform results, no recursion),
// returns true if every test passed
bool runRulesetContractTests();
