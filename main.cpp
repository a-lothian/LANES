#include <iostream>
#include <string>

#include "codegen.hpp"
#include "file_handler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

static std::string outputFilePath(const std::string &inputPath)
{
    std::size_t dot = inputPath.find_last_of('.');
    std::size_t slash = inputPath.find_last_of("/\\");

    // only strip strip file extension
    bool hasExtension = dot != std::string::npos && (slash == std::string::npos || dot > slash);

    return (hasExtension ? inputPath.substr(0, dot) : inputPath) + ".wat";
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

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: lanes <input file>\n";
        return 1;
    }

    std::string inputPath = argv[1];

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

    if (!files.writeFile(outputFilePath(inputPath), wat))
    {
        std::cerr << files.getError() << "\n";
        return 1;
    }

    return 0;
}