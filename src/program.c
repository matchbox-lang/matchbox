#include "program.h"
#include "buffer.h"
#include "compiler.h"
#include "module_object.h"
#include "repl.h"
#include "vm.h"
#include <stdbool.h>
#include <stdlib.h>

static void runModule(ModuleObject* module, bool disassemble)
{
    if (disassemble) {
        disassembleModule(module);
        return;
    }

    VM vm;

    initVM(&vm, module);
    interpret(&vm);
    freeVM(&vm);
}

static void runSource(char* source, bool disassemble)
{
    ModuleObject* module = createModuleObject();
    Compiler compiler;
    
    initCompiler(&compiler, module);
    compile(&compiler, source);
    runModule(module, disassemble);

    freeCompiler(&compiler);
    freeModuleObject(module);
}

static void runFile(Options* options)
{
    char* source = getFileContents(options->filename);
    
    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", options->filename);
        exit(1);
    }

    runSource(source, options->disassemble);
    free(source);
}

void printUsage(FILE* stream)
{
    fprintf(stream, "Usage: %s [options] [--] [file]\n", PROGRAM_COMMAND);
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -d, --disassemble     Print bytecode without running the program\n");
    fprintf(stream, "  -h, --help            Show help options\n");
    fprintf(stream, "      --version         Show version information\n");
}

void printVersion()
{
    printf("%s\n", PROGRAM_VERSION);
}

void runProgram(Options* options)
{
    if (!options->filename) {
        runRepl();

        return;
    }

    runFile(options);
}
