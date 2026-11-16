# LANES Rubric Features

This document lists the features of LANES against the assignment rubric, and where each can be found in the source and tests.

The unit-test suite covers the lexer, parser, semantic analysis, code generation and execution. It can be run with:

```
make test // run all tests & print results
```


## Basic Functionality

- **Variables:** Implemented

Variables are declared with `let`, either with an inferred type (`let x = 5;`) or an annotated one (`let x: float = 5.0;`). They can be reassigned, and the new value must match the variable's type.
- **Literals:** Implemented

 Integers (`42`), floats (`3.5`), booleans (`true`, `false`) and field literals (`[1.0, 2.0, x * 2.0]`).
- **Arithmetic:** Implemented

 `+ - * /` and unary `-`, along with comparisons (`== != < > <= >=`) and logical operators (`and or not`), using standard precedence (`src/parser.cpp`). Inside rulesets, all of these also work element-wise on fields (dividing requires float fields).
- **Selection:** Implemented

 `if`, `else if` and `else`. Fields also have `select(mask, a, b)`, which picks between two values element by element.
- **Iteration:** Implemented

 `while` loops, and `for (i in start, end)` loops over a range of integers.
- **Functions:** Implemented

 Functions take parameters, return values and can be recursive (`tests/exec/input/control_flow.txt` computes factorials). Built-in functions include `min`, `max`, `clamp`, `abs`, `sqrt`, `floor`, `ceil`, `round`, `to_f32`, `to_i32` and `print`.
- **Multi-element variables (arrays):** Implemented

 The `field<T, N>` type holds `N` values of `f32`, `i32` or `bool`. Fields can be created from literals, passed to and returned from functions, operated on element-wise inside rulesets, reduced to a single value, and printed. Fields are copied when assigned or passed (`tests/exec/input/field_values.txt`).
- **Scope:** Implemented. 

Each block opens a new scope in the symbol table (`src/symbol_table.cpp`). Inner variables can shadow outer ones (`tests/codegen/generator/input/shadowing.txt`). A name cannot be declared twice in the same scope. A `for` loop variable only exists inside its loop, and cannot be assigned to.


## Advanced Functionality

- **Object orientation and encapsulation:** Not implemented

 LANES is not object oriented.
- **Polymorphism:** Partially implemented.

Operators and built-in functions are overloaded by type, so `+` and `min` work on integers, floats and fields of either. The compiler picks the matching WebAssembly instructions for each. There is no user-defined polymorphism.
- **Generics:** Partially implemented.

 `field<T, N>` is a built-in generic type, parameterised by its element type and length, both checked at compile time. Users cannot define their own generic types or functions.
- **Static memory allocation:** Implemented. 

Every field is given a fixed, 16-byte aligned address in WebAssembly memory at compile time (`src/memory_layout.cpp`). The compiler works out how much memory the program needs, and reports an error if it won't fit in 4 GB.
- **Dynamic memory allocation and garbage collection:** Not implemented. 

All memory is laid out at compile time.


## Basic Compilation Process

- **Tokenisation:** Implemented. 

`src/lexer.cpp` turns the source text into tokens (`include/token.hpp`), skipping whitespace and `//` comments.
- **Lexical analysis:** Implemented. 

The lexer recognises keywords, names, integer and float literals, and one and two character operators (`->`, `==`, `<=`). Invalid characters produce error tokens. There are 38 lexer test inputs in `tests/lexer/input`.
- **Syntax analysis:** Implemented. 

`src/parser.cpp` is a recursive-descent parser, which builds an abstract syntax tree (`include/ast.hpp`) of declarations, statements, types and expressions.
- **Code generation:** Implemented. 

`src/codegen.cpp` walks the syntax tree and outputs WAT through `src/wat_emitter.cpp`. WAT2WASM then translates it into a `.wasm` binary.


## Advanced Compilation Process

### Semantic Analysis

Implemented in `src/semantic.cpp`. Programs are checked for the following before any code is generated:

- **Type checking:** 

Every expression's type is inferred and checked. Mismatched operands, assignments, return values and function arguments are reported, conditions must be booleans, and logical operators require boolean operands.
- **Avoiding mathematical errors:** 

Arithmetic requires numeric operands, and float-only built-ins such as `sqrt` require floats. Dividing integer fields is rejected, as WebAssembly SIMD has no integer division. Converting between integers and floats must be done explicitly with `to_f32` and `to_i32`.
- **Order of instantiation:** 

Variables must be declared before they are used. Functions and rulesets can be called before they are defined, as they are all registered in a first pass. A `for` loop's range is checked before its loop variable exists.
- **Return checking:** 

A function that returns a value must return on every path, and a `void` function cannot return a value.
- **Ruleset restrictions:** 

Rulesets are compiled into SIMD loops, so they must have a single `return` as their last statement, and cannot contain `if`, `while`, `for`, `print`, nested blocks, recursion or calls to ordinary functions. A ruleset that returns a field cannot use reductions, and one that returns a single value must use a reduction (or call a ruleset that does) to produce it.
- **Other errors:** 

Undefined names, duplicate declarations and parameters, the wrong number of arguments, assigning to a loop variable, redefining a built-in, and names starting with `__`, which are reserved for the compiler.

### Optimisation

Partially implemented. The compiler makes optimisations specific to element-wise maths over large fields (see Domain-Specific Optimisations below). There are no general optimisations such as dead code elimination, constant folding or loop unrolling.

### Concurrency and Parallelism

Partially implemented. LANES is built around data parallelism: all work on fields is compiled to WebAssembly SIMD instructions, which process 4 values at once. There is no support for multi-threading.

### Domain-Specific Optimisations

Implemented in `src/codegen.cpp`. Each ruleset is compiled into a single SIMD loop:

- **Loop fusion:** 

All of a ruleset's statements run in one loop, so `let y = a * b; return y + c;` makes a single pass over memory, without storing `y` in between.
- **Hoisting uniforms:** 

Single values used by field operations are calculated once before the loop and copied into all 4 lanes, rather than recalculated on every iteration.
- **Vectorised reductions:** 

`sum`, `min_of`, `max_of`, `any` and `all` keep 4 running results through the loop, and combine them once at the end. Padding at the end of a field is masked out so it does not affect the result.
- **No remainder loop:** 

Fields are padded to a multiple of 4 values, so the last few values never need a separate, slower loop.
- **No input copies:** 

Rulesets read their field parameters directly from the caller's memory, so inputs are never copied or overwritten. Ordinary functions only copy a field parameter if they assign to it.