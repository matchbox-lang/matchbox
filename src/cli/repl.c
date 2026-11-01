#include "cli/repl.h"
#include "common/errors.h"
#include "common/io.h"
#include "common/memory.h"
#include "compiler/compiler.h"
#include "vm/vm.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum SubmissionResult
{
    SubmissionReset = -1,
    SubmissionIncomplete,
    SubmissionComplete,
    SubmissionExit
} SubmissionResult;

static void unexpected_end_of_input_error(Token token)
{
    report_error("Unexpected end of input on line %d:%d", token.line, token.column);
}

static bool append_line(char** source, size_t* size, size_t* length)
{
    char* line = NULL;
    size_t line_size = 0;
    int count = read_stream_line(&line, &line_size, stdin);

    if (count < 0) {
        free_memory(line);

        return false;
    }

    size_t required = *length + (size_t)count + 1;

    if (required > *size) {
        *source = reallocate_memory(*source, required);
        *size = required;
    }

    memcpy(*source + *length, line, (size_t)count + 1);
    free_memory(line);

    *length += (size_t)count;

    return true;
}

static SubmissionResult process_source(Compiler* compiler, VM* vm, const char* source, MemoryGroup* memory)
{
    ErrorContext context;

    begin_error_context(&context, memory);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);

        return SubmissionReset;
    }

    CompileResult result = compile_repl(compiler, source);

    if (result == CompileIncomplete) {
        end_error_context(&context);

        return SubmissionIncomplete;
    }

    if (result == CompileError) {
        end_error_context(&context);

        return SubmissionComplete;
    }

    resize_value_array(&vm->globals, vm->module->global_count);

    vm->fp = vm->stack;
    vm->ip = NULL;

    interpret(vm);
    end_error_context(&context);

    if (vm->exit_requested) {
        return SubmissionExit;
    }

    return SubmissionComplete;
}

static void finish_submission(SubmissionResult result, char** source, size_t* size, size_t* length)
{
    if (result == SubmissionReset || result == SubmissionExit) {
        return;
    }

    if (result == SubmissionComplete) {
        *source = NULL;
        *size = 0;
        *length = 0;
    }

    if (*length == 0) {
        printf(">>> ");
    } else {
        printf("... ");
    }

    fflush(stdout);
}

static SubmissionResult finish_input(Compiler* compiler, size_t length, SubmissionResult result)
{
    if (result == SubmissionReset || result == SubmissionExit) {
        return result;
    }

    printf("\n");

    if (length == 0) {
        return SubmissionComplete;
    }

    unexpected_end_of_input_error(compiler->parser.current_token);

    return SubmissionIncomplete;
}

static SubmissionResult run_session(int* status)
{
    MemoryGroup memory = {0};
    MemoryGroup* previous = use_memory_group(&memory);
    ModuleObject* module = create_module_object();
    char* source = NULL;
    size_t size = 0;
    size_t length = 0;
    SubmissionResult result = SubmissionComplete;
    Compiler compiler;
    VM vm;

    init_compiler(&compiler, module);
    init_vm(&vm, module);
    reserve_value_array(&vm.globals, UINT16_MAX + 1u);
    finish_submission(SubmissionIncomplete, &source, &size, &length);

    while (result != SubmissionReset && result != SubmissionExit && append_line(&source, &size, &length)) {
        result = process_source(&compiler, &vm, source, &memory);

        finish_submission(result, &source, &size, &length);
    }

    *status = vm.exit_status;
    result = finish_input(&compiler, length, result);

    use_memory_group(previous);
    free_memory_group(&memory);

    return result;
}

int run_repl(void)
{
    SubmissionResult result = SubmissionReset;
    int status = EXIT_SUCCESS;

    while (result == SubmissionReset) {
        result = run_session(&status);
    }

    if (result == SubmissionExit) {
        return status;
    }

    return result == SubmissionComplete ? EXIT_SUCCESS : EXIT_FAILURE;
}
