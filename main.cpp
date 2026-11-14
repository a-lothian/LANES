#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "codegen.hpp"
#include "file_handler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

static std::string outputBasePath(const std::string &inputPath)
{
    std::size_t dot = inputPath.find_last_of('.');
    std::size_t slash = inputPath.find_last_of("/\\");

    // only strip strip file extension
    bool hasExtension = dot != std::string::npos && (slash == std::string::npos || dot > slash);

    return hasExtension ? inputPath.substr(0, dot) : inputPath;
}

static bool compile(const std::string &source, std::string &wat)
{
    Lexer lexer(source);
    lexer.tokenize();

    Program program;
    try
    {
        Parser parser(lexer.get_tokens(), source);
        program = parser.parse();
    }
    catch (const ParseError &error)
    {
        std::cerr << "syntax error: " << error.what() << "\n";
        return false;
    }

    SemanticAnalyser analyser;
    if (!analyser.analyse(program))
    {
        for (const std::string &error : analyser.getErrors())
        {
            std::cerr << "error: " << error << "\n";
        }
        return false;
    }

    CodeGenerator generator;
    if (!generator.generate(program))
    {
        for (const std::string &error : generator.getErrors())
        {
            std::cerr << "error: " << error << "\n";
        }
        return false;
    }

    wat = generator.getWat();
    return true;
}

// translates WAT into binary .wasm file using wat2wasm
static bool assembleWasm(const std::string &watPath, const std::string &wasmPath)
{
    std::string command = "wat2wasm \"" + watPath + "\" -o \"" + wasmPath + "\"";
    return std::system(command.c_str()) == 0;
}

int main(int argc, char **argv)
{
    bool textOutput = false;
    std::string inputPath;

    for (int i = 1; i < argc; i++)
    {
        std::string argument = argv[i];
        if (argument == "-t" && !textOutput)
        {
            textOutput = true;
        }
        else if (inputPath.empty())
        {
            inputPath = argument;
        }
        else
        {
            inputPath.clear();
            break;
        }
    }

    if (inputPath.empty())
    {
        std::cerr << "usage: lanes [-t] <input file>\n"
                  << "  default: writes <name>.wasm (needs wat2wasm, see 'make install')\n"
                  << "  -t     : writes <name>.wat as text instead\n";
        return 1;
    }

    FileHandler files;
    if (!files.readFile(inputPath))
    {
        std::cerr << files.getError() << "\n";
        return 1;
    }

    std::string wat;
    if (!compile(files.document, wat))
    {
        return 1;
    }

    std::string basePath = outputBasePath(inputPath);
    std::string watPath = basePath + ".wat";

    if (!files.writeFile(watPath, wat))
    {
        std::cerr << files.getError() << "\n";
        return 1;
    }

    if (textOutput)
    {
        return 0;
    }

    bool assembled = assembleWasm(watPath, basePath + ".wasm");
    std::remove(watPath.c_str());

    if (!assembled)
    {
        std::cerr << "could not run wat2wasm: install it with 'make install', or use -t for WAT output\n";
        return 1;
    }
    return 0;
}
