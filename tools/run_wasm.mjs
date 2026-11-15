// runs a compiled LANES program
//
// usage: node tools/run_wasm.mjs <program.wasm>
// calls the exported `main` function

import { readFile } from "node:fs/promises";

const path = process.argv[2];
if (!path) {
    console.error("usage: node tools/run_wasm.mjs <program.wasm>");
    process.exit(2);
}

// shortest decimal that reads back as the same 32-bit float
// JavaScript numbers are 64-bit, so 0.1 as an f32 would otherwise print as 0.10000000149011612
function formatF32(value) {
    if (Number.isNaN(value)) return "nan";
    if (value === Infinity) return "inf";
    if (value === -Infinity) return "-inf";
    for (let digits = 1; digits <= 9; digits++) {
        const text = value.toPrecision(digits);
        if (Math.fround(Number(text)) === value) {
            return String(Number(text));
        }
    }
    return String(value);
}

const formatI32 = (value) => String(value);
// bools are 0 / 1 as scalars, but 0 / -1 inside fields (SIMD masks), so any non-zero is true
const formatBool = (value) => (value !== 0 ? "true" : "false");

let memory = null;

// reads `length` elements starting at `address` from the module's exported memory
function printField(ArrayType, format) {
    return (address, length) => {
        if (memory === null) {
            throw new Error("program prints a field but does not export its memory");
        }
        const elements = Array.from(new ArrayType(memory.buffer, address, length), format);
        console.log(`[${elements.join(", ")}]`);
    };
}

const imports = {
    env: {
        print_i32: (value) => console.log(formatI32(value)),
        print_f32: (value) => console.log(formatF32(value)),
        print_bool: (value) => console.log(formatBool(value)),
        print_field_i32: printField(Int32Array, formatI32),
        print_field_f32: printField(Float32Array, formatF32),
        print_field_bool: printField(Int32Array, formatBool),
    },
};

const { instance } = await WebAssembly.instantiate(await readFile(path), imports);
memory = instance.exports.memory ?? null;

const main = instance.exports.main;
if (typeof main !== "function") {
    console.error("program has no exported main function");
    process.exit(1);
}

try {
    main();
} catch (error) {
    if (error instanceof WebAssembly.RuntimeError) {
        console.log(`trap: ${error.message}`);
        process.exit(1);
    }
    throw error;
}
