#include "cli/program.h"
#include "cli/check_runner.h"
#include "cli/options.h"
#include "cli/repl.h"
#include "cli/program_test_runner.h"
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

static _Noreturn void read_file_error(const char* filename)
{
    const char* detail = strerror(errno);

    fatal_error("Could not read file %s: %s", filename, detail);
}

static int run_module(ModuleObject* module, bool disassemble)
{
    if (disassemble) {
        disassemble_module(module);

        return EXIT_SUCCESS;
    }

    VM vm;

    init_vm(&vm, module);

    int status = interpret(&vm);

    free_vm(&vm);

    return status;
}

static int run_source(const char* source, bool disassemble)
{
    ModuleObject* module = create_module_object();
    Compiler compiler;
    
    init_compiler(&compiler, module);
    if (!compile(&compiler, source)) {
        unexpected_end_of_input_error(compiler.parser.current_token);
    }

    int status = run_module(module, disassemble);

    free_compiler(&compiler);
    free_module_object(module);

    return status;
}

static int run_file(const Options* options)
{
    char* source = get_file_contents(options->filename);
    
    if (!source) {
        read_file_error(options->filename);
    }

    int status = run_source(source, options->disassemble);

    free_memory(source);

    return status;
}

void print_usage(FILE* stream)
{
    fprintf(stream, "Usage: %s [options] [--] [file]\n", PROGRAM_COMMAND);
    fprintf(stream, "       %s check [path]\n", PROGRAM_COMMAND);
    fprintf(stream, "       %s test [-a | -p] [--program] [path]\n\n", PROGRAM_COMMAND);
    fprintf(stream, "Commands:\n");
    fprintf(stream, "  check [path]         Check source code without building\n");
    fprintf(stream, "  test [path]          Run test blocks\n");
    fprintf(stream, "\n");
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -d, --disassemble    Print bytecode without running the program\n");
    fprintf(stream, "  -h, --help           Show help information\n");
    fprintf(stream, "      --version        Show version information\n");
    fprintf(stream, "\nTest options:\n");
    fprintf(stream, "  -a                   Show all test results\n");
    fprintf(stream, "  -p                   Show passed test results\n");
    fprintf(stream, "      --program        Run program tests\n");
}

void print_version()
{
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

static int run_test_command(const Options* options)
{
    if (options->programs) {
        return run_program_tests(options);
    }

    return run_tests(options);
}

int run_program(const Options* options)
{
    if (options->mode == ProgramCheck) {
        return run_checks(options);
    }

    if (options->mode == ProgramTest) {
        return run_test_command(options);
    }

    if (!options->filename) {
        return run_repl();
    }

    return run_file(options);
}
