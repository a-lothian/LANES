# LANES

A language and compiler built for the final assignment of *2650_COMP_2001_2*.

![LANES and JavaScript running the same demos side by side](assets/demo.gif)


LANES is a statically typed programming language designed to make SIMD (Single Instruction, Multiple Data) programming easier. SIMD allows efficient processing of large volumes of data by applying an operation over multiple registers in a single CPU instruction. LANES compiles to WAT (and WASM), enabling performant computation in modern web browsers.


## Getting Started

The LANES compiler outputs WAT (WebAssembly Text) by default, and uses WAT2WASM to translate it into machine code. This dependency can be installed with:

```
make install // installs wat2wasm via brew / apt-get
```

The compiler can then be built, and the demo can be compiled. Once completed, open it in your browser to see LANES run.

```
make demo // builds compiler, then compiles demo. outputs 'demo/benchmarks.html'.
```

The compiler can also be built without compiling the demo:

```
make // outputs to 'build/'
```


The unit-test suite can be run with:

```
make test // run all tests & print results
```
All build artifacts can be removed with:
```
make clean // remove all build artifacts
```
