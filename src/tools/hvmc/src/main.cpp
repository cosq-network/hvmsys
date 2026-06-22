#include "hvmc/types.h"
#include "hvmc/encoder.h"
#include "hvmc/elf.h"
#include "hvmc/assembler.h"
#include "hvmc/lexer.h"
#include "hvmc/parser.h"
#include "hvmc/codegen.h"
#include "hvmc/preprocessor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>

using namespace hvmc;

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] input\n";
    std::cerr << "Options:\n";
    std::cerr << "  -o FILE    Output file (default: a.out)\n";
    std::cerr << "  -S         Produce assembly output\n";
    std::cerr << "  -c         Produce object file only\n";
    std::cerr << "  -E         Preprocess only\n";
    std::cerr << "  -I PATH    Add include path\n";
    std::cerr << "  -v         Verbose\n";
    std::cerr << "  --help     Show this help\n";
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Error: cannot open " << path << "\n";
        std::exit(1);
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void write_file(const std::string& path, const std::vector<u8>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Error: cannot write " << path << "\n";
        std::exit(1);
    }
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
}

static bool has_suffix(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string output = "a.out";
    bool emit_asm = false;
    bool emit_obj = false;
    bool emit_preproc = false;
    bool verbose = false;
    std::vector<std::string> include_paths;
    std::string input;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) output = argv[++i];
        else if (arg == "-S") emit_asm = true;
        else if (arg == "-c") emit_obj = true;
        else if (arg == "-E") emit_preproc = true;
        else if (arg == "-v") verbose = true;
        else if (arg == "-I" && i + 1 < argc) { include_paths.push_back(argv[++i]); }
        else if (arg == "--help") { usage(argv[0]); return 0; }
        else input = arg;
    }

    if (input.empty()) {
        std::cerr << "Error: no input file specified\n";
        return 1;
    }

    std::string source = read_file(input);

    // Handle preprocessor only mode
    if (emit_preproc) {
        std::string preprocessed = preprocess(source, include_paths);
        
        // Determine output name
        if (output == "a.out") {
            output = input + ".i";
        }
        
        std::ofstream f(output);
        if (!f) {
            std::cerr << "Error: cannot write " << output << "\n";
            return 1;
        }
        f << preprocessed;
        f.close();
        
        if (verbose) {
            std::cerr << "Preprocessed: " << input << " -> " << output << "\n";
        }
        return 0;
    }

    // Preprocess the source if it's a C file
    if (has_suffix(input, ".c")) {
        source = preprocess(source, include_paths);
    }

    // Determine default output name if not specified
    if (output == "a.out") {
        if (emit_asm) {
            output = input + ".s";
        } else if (emit_obj) {
            output = input + ".o";
        } else {
            output = "a.elf";
        }
    }

    try {
        if (has_suffix(input, ".s") || has_suffix(input, ".asm")) {
            // Assembly file: assemble directly
            if (verbose)
                std::cerr << "Assembling: " << input << "\n";

            ObjectFile obj = assemble(source, input);

            std::vector<u8> elf = write_elf(obj);
            write_file(output, elf);

            if (verbose)
                std::cerr << "Wrote: " << output << " (" << elf.size() << " bytes)\n";
        }
        else if (has_suffix(input, ".c")) {
            // C file: compile
            if (verbose)
                std::cerr << "Compiling: " << input << "\n";

            if (emit_asm) {
                // Parse and generate assembly
                Parser parser(source);
                ASTNode* ast = parser.parse();

                CodeGen cg;
                std::string asm_out = cg.gen(ast);

                std::ofstream f(output);
                if (!f) {
                    std::cerr << "Error: cannot write " << output << "\n";
                    return 1;
                }
                f << asm_out;
                f.close();

                if (verbose)
                    std::cerr << "Wrote assembly: " << output << "\n";
            } else {
                // Full compilation: C -> assembly -> object
                Parser parser(source);
                ASTNode* ast = parser.parse();

                CodeGen cg;
                std::string asm_source = cg.gen(ast);

                // Assemble the generated assembly
                ObjectFile obj = assemble(asm_source, input + ".s");
                std::vector<u8> elf;

                if (emit_obj) {
                    elf = write_elf(obj);
                } else {
                    elf = write_executable(obj);
                }

                write_file(output, elf);

                if (verbose)
                    std::cerr << "Wrote: " << output << " (" << elf.size() << " bytes)\n";
            }
        } else {
            std::cerr << "Error: unknown file type: " << input << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
