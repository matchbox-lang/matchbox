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
    SUBMISSION_RESET = -1,
    SUBMISSION_INCOMPLETE,
    SUBMISSION_COMPLETE,
    SUBMISSION_EXIT
} SubmissionResult;

static void unexpectedEndOfInputError(Token token)
{
    reportError("Unexpected end of input on line %d:%d", token.line, token.column);
}

static bool appendLine(char** source, size_t* size, size_t* length)
{
    char* line = NULL;
    size_t lineSize = 0;
    int count = readStreamLine(&line, &lineSize, stdin);

    if (count < 0) {
        freeMemory(line);

        return false;
    }

    size_t required = *length + (size_t)count + 1;

    if (required > *size) {
        *source = reallocateMemory(*source, required);
        *size = required;
    }

    memcpy(*source + *length, line, (size_t)count + 1);
    freeMemory(line);

    *length += (size_t)count;

    return true;
}

static SubmissionResult processSource(Compiler* compiler, VM* vm, const char* source, MemoryGroup* memory)
{
    ErrorContext context;

    beginErrorContext(&context, memory);

    if (setjmp(context.jump) != 0) {
        endErrorContext(&context);

        return SUBMISSION_RESET;
    }

    CompileResult result = compileRepl(compiler, source);

    if (result == COMPILE_INCOMPLETE) {
        endErrorContext(&context);

        return SUBMISSION_INCOMPLETE;
    }

    if (result == COMPILE_ERROR) {
        endErrorContext(&context);

        return SUBMISSION_COMPLETE;
    }

    resizeValueArray(&vm->globals, vm->module->globalCount);

    vm->fp = vm->stack;
    vm->ip = NULL;

    interpret(vm);
    endErrorContext(&context);

    if (vm->exitRequested) {
        return SUBMISSION_EXIT;
    }

    return SUBMISSION_COMPLETE;
}

static void finishSubmission(SubmissionResult result, char** source, size_t* size, size_t* length)
{
    if (result == SUBMISSION_RESET || result == SUBMISSION_EXIT) {
        return;
    }

    if (result == SUBMISSION_COMPLETE) {
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

static SubmissionResult finishInput(Compiler* compiler, size_t length, SubmissionResult result)
{
    if (result == SUBMISSION_RESET || result == SUBMISSION_EXIT) {
        return result;
    }

    printf("\n");

    if (length == 0) {
        return SUBMISSION_COMPLETE;
    }

    unexpectedEndOfInputError(compiler->parser.currentToken);

    return SUBMISSION_INCOMPLETE;
}

static SubmissionResult runSession(int* status)
{
    MemoryGroup memory = {0};
    MemoryGroup* previous = useMemoryGroup(&memory);
    ModuleObject* module = createModuleObject();
    char* source = NULL;
    size_t size = 0;
    size_t length = 0;
    SubmissionResult result = SUBMISSION_COMPLETE;
    Compiler compiler;
    VM vm;

    initCompiler(&compiler, module);
    initVM(&vm, module);
    reserveValueArray(&vm.globals, UINT16_MAX + 1u);
    finishSubmission(SUBMISSION_INCOMPLETE, &source, &size, &length);

    while (result != SUBMISSION_RESET && result != SUBMISSION_EXIT && appendLine(&source, &size, &length)) {
        result = processSource(&compiler, &vm, source, &memory);

        finishSubmission(result, &source, &size, &length);
    }

    *status = vm.exitStatus;
    result = finishInput(&compiler, length, result);

    useMemoryGroup(previous);
    freeMemoryGroup(&memory);

    return result;
}

int runRepl(void)
{
    SubmissionResult result = SUBMISSION_RESET;
    int status = EXIT_SUCCESS;

    while (result == SUBMISSION_RESET) {
        result = runSession(&status);
    }

    if (result == SUBMISSION_EXIT) {
        return status;
    }

    return result == SUBMISSION_COMPLETE ? EXIT_SUCCESS : EXIT_FAILURE;
}
