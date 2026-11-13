
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <exception>

#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include "codegen.hpp"

int main(int argc, char *argv[])
{
    // Check that a LANES source file was provided.
    if (argc < 2)
    {
        std::cerr << "Usage: ./build/temp_lanes <source-file> [output.wat]\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = "output.wat";

    // Allow the user to choose the output file.
    if (argc >= 3)
    {
        outputPath = argv[2];
    }

    // STEP 1: Read the LANES source file.
    std::ifstream inputFile(inputPath);

    if (!inputFile.is_open())
    {
        std::cerr << "Cannot open file: " << inputPath << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();

    std::string source = buffer.str();

    std::cout << "Source loaded: " << inputPath << "\n";

    try
    {
        // STEP 2: Convert source code into tokens.
        Lexer lexer(source);
        lexer.tokenize();

        std::vector<Token> tokens = lexer.get_tokens();

        std::cout << "Lexer: OK\n";

        // STEP 3: Convert tokens into an AST.
        Parser parser(tokens, source);
        Program program = parser.parse();

        std::cout << "Parser: OK\n";

        // STEP 4: Check variable types, scopes,
        // function calls and other semantic rules.
        SemanticAnalyser semantic;

        if (!semantic.analyse(program))
        {
            std::cerr << "Semantic analysis failed:\n";

            const std::vector<std::string> &errors =
                semantic.getErrors();

            for (std::size_t i = 0; i < errors.size(); i++)
            {
                std::cerr << "  " << errors[i] << "\n";
            }

            return 1;
        }

        std::cout << "Semantic analysis: OK\n";

        // STEP 5: Generate WebAssembly Text.
        CodeGenerator generator;

        if (!generator.generate(program))
        {
            std::cerr << "Code generation failed:\n";

            const std::vector<std::string> &errors =
                generator.getErrors();

            for (std::size_t i = 0; i < errors.size(); i++)
            {
                std::cerr << "  " << errors[i] << "\n";
            }

            return 1;
        }

        std::cout << "Code generation: OK\n";

        // STEP 6: Save the generated WAT to a file.
        std::ofstream outputFile(outputPath);

        if (!outputFile.is_open())
        {
            std::cerr << "Cannot create file: "
                      << outputPath << "\n";
            return 1;
        }

        outputFile << generator.getWat();

        if (!outputFile.good())
        {
            std::cerr << "Failed to write: "
                      << outputPath << "\n";
            return 1;
        }

        outputFile.close();

        std::cout << "WAT saved to: " << outputPath << "\n";
        std::cout << "Compilation successful!\n";
    }
    catch (const ParseError &error)
    {
        std::cerr << "Parser error: " << error.what() << "\n";
        return 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Compiler error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
