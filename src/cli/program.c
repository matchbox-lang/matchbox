#include "cli/program.h"
#include "cli/check_runner.h"
#include "cli/options.h"
#include "cli/repl.h"
#include "cli/test_runner.h"
#include "common/errors.h"
#include "common/io.h"
#include "common/memory.h"
#include "compiler/compiler.h"
#include "compiler/diagnostics.h"
#include "config.h"
#include "runtime/module_object.h"
#include "vm/disassembler.h"
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

static int runModule(ModuleObject* module, bool disassemble)
{
    if (disassemble) {
        disassembleModule(module);

        return EXIT_SUCCESS;
    }

    VM vm;

    initVM(&vm, module);

    int status = interpret(&vm);

    freeVM(&vm);

    return status;
}

static int runSource(char* source, bool disassemble)
{
    ModuleObject* module = createModuleObject();
    Compiler compiler;
    
    initCompiler(&compiler, module);
    if (!compile(&compiler, source)) {
        unexpectedEndOfInputError(compiler.parser.currentToken);
    }

    int status = runModule(module, disassemble);

    freeCompiler(&compiler);
    freeModuleObject(module);

    return status;
}

static int runFile(Options* options)
{
    char* source = getFileContents(options->filename);
    
    if (!source) {
        readFileError(options->filename);
    }

    int status = runSource(source, options->disassemble);

    freeMemory(source);

    return status;
}

void printUsage(FILE* stream)
{
    fprintf(stream, "Usage: %s [options] [--] [file]\n", PROGRAM_COMMAND);
    fprintf(stream, "       %s check [path]\n", PROGRAM_COMMAND);
    fprintf(stream, "       %s test [-a | -p] [path]\n\n", PROGRAM_COMMAND);
    fprintf(stream, "Commands:\n");
    fprintf(stream, "  check [path]          Check source code for errors without building\n");
    fprintf(stream, "                        Recursively check .mb files (default path: .)\n");
    fprintf(stream, "                        Skip discovered tests/ directories; explicit paths are included\n");
    fprintf(stream, "  test [path]           Run test files\n");
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -d, --disassemble     Print bytecode without running the program\n");
    fprintf(stream, "  -h, --help            Show help information\n");
    fprintf(stream, "      --version         Show version information\n");
    fprintf(stream, "\nTest options:\n");
    fprintf(stream, "  -a                    Show all test results\n");
    fprintf(stream, "  -p                    Show passed test results\n");
}

void printVersion()
{
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

int runProgram(Options* options)
{
    if (options->mode == PROGRAM_CHECK) {
        return runChecks(options);
    }

    if (options->mode == PROGRAM_TEST) {
        return runTests(options);
    }

    if (!options->filename) {
        return runRepl();
    }

    return runFile(options);
}
