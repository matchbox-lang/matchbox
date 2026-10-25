#include "program/program.h"
#include "common/memory.h"
#include "compiler/diagnostics.h"
#include "common/errors.h"
#include "program/io.h"
#include "compiler/compiler.h"
#include "runtime/module_object.h"
#include "program/options.h"
#include "program/repl.h"
#include "program/test_runner.h"
#include "vm/vm.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static _Noreturn void readFileError(const char* filename)
{
    const char* detail = strerror(errno);

    fatalError("Could not read file %s: %s", filename, detail);
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
        unexpectedEndOfInputError(compiler.parser.currentToken);
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
    freeMemory(source);

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
        return runTests(options);
    }

    if (!options->filename) {
        return runRepl();
    }

    return runFile(options);
}
