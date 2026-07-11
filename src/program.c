#include "program.h"
#include "buffer.h"
#include "compiler.h"
#include "module_object.h"
#include "options.h"
#include "repl.h"
#include "test_runner.h"
#include "vm.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void readFileError(const char* filename)
{
    fprintf(stderr, "Error: Could not read file %s: %s\n", filename, strerror(errno));
    exit(1);
}

static void unexpectedEndError(Token token)
{
    fprintf(stderr, "Error: Unexpected end of input on line %d:%d\n", token.line, token.column);
    exit(1);
}

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
    if (!compile(&compiler, source)) {
        unexpectedEndError(compiler.parser.currentToken);
    }
    runModule(module, disassemble);

    freeCompiler(&compiler);
    freeModuleObject(module);
}

static bool runFile(Options* options)
{
    char* source = getFileContents(options->filename);
    
    if (!source) {
        readFileError(options->filename);
    }

    runSource(source, options->disassemble);
    free(source);

    return true;
}

void printUsage(FILE* stream)
{
    fprintf(stream, "Usage: %s [options] [--] [file]\n", PROGRAM_COMMAND);
    fprintf(stream, "       %s test [-a | -p] [path]\n\n", PROGRAM_COMMAND);
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -d, --disassemble     Print bytecode without running the program\n");
    fprintf(stream, "  -h, --help            Show help options\n");
    fprintf(stream, "      --version         Show version information\n");
    fprintf(stream, "\nTest options:\n");
    fprintf(stream, "  -a                    Show all test results\n");
    fprintf(stream, "  -p                    Show passed test results\n");
}

void printVersion()
{
    printf("%s\n", PROGRAM_VERSION);
}

bool runProgram(Options* options)
{
    if (options->mode == PROGRAM_TEST) {
        runTests(options);

        return true;
    }

    if (!options->filename) {
        runRepl();

        return true;
    }

    return runFile(options);
}
