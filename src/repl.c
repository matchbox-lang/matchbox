#include "repl.h"
#include "buffer.h"
#include "compiler.h"
#include "module_object.h"
#include "util.h"
#include "vector.h"
#include "vm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool resizeSource(char** source, size_t* size, size_t required)
{
    char* resized = realloc(*source, required);
    if (!resized) {
        return false;
    }

    *source = resized;
    *size = required;

    return true;
}

static bool appendLine(char** source, size_t* size, size_t* length)
{
    char* line = NULL;
    size_t lineSize = 0;
    int lineLength = getStreamContents(&line, &lineSize, stdin);

    if (lineLength == -1) {
        free(line);

        return false;
    }

    size_t required = *length + (size_t)lineLength + 1;
    bool resizeFailed = required > *size && !resizeSource(source, size, required);

    if (resizeFailed) {
        free(line);

        return false;
    }

    memcpy(*source + *length, line, (size_t)lineLength + 1);
    *length += (size_t)lineLength;
    free(line);

    return true;
}

static void unexpectedEndError(Compiler* compiler)
{
    Token token = compiler->parser.currentToken;

    fprintf(stderr, "Error: Unexpected end of input on line %d:%d\n", token.line, token.column);
    exit(1);
}

static bool finishSourceRead(Compiler* compiler, bool read, size_t length)
{
    if (read) {
        return true;
    }

    printf("\n");

    if (length > 0) {
        unexpectedEndError(compiler);
    }

    return false;
}

static bool readSource(Compiler* compiler, char** source, size_t* size)
{
    size_t length = 0;

    printf(">>> ");
    bool read = appendLine(source, size, &length);
    bool complete = read && compileRepl(compiler, *source);

    while (read && !complete) {
        printf("... ");
        read = appendLine(source, size, &length);
        complete = read && compileRepl(compiler, *source);
    }

    return finishSourceRead(compiler, read, length);
}

static void freeReplSources(Vector* sources)
{
    size_t count = countVector(sources);

    for (size_t i = 0; i < count; i++) {
        free(getVectorAt(sources, i));
    }

    freeVector(sources);
}

static void appendReplHistory(
    char** history, size_t* size, size_t* length, const char* source)
{
    size_t sourceLength = strlen(source);
    size_t required = *length + sourceLength + 1;

    if (required > *size && !resizeSource(history, size, required)) {
        outOfMemoryError();
    }

    memcpy(*history + *length, source, sourceLength + 1);
    *length += sourceLength;
}

static void validateReplHistory(char* history)
{
    ModuleObject* module = createModuleObject();
    Compiler compiler;

    initCompiler(&compiler, module);

    if (!compile(&compiler, history)) {
        unexpectedEndError(&compiler);
    }

    freeCompiler(&compiler);
    freeModuleObject(module);
}

void runRepl()
{
    char* source = NULL;
    size_t size = 0;
    char* history = NULL;
    size_t historySize = 0;
    size_t historyLength = 0;
    ModuleObject* module = createModuleObject();
    Compiler compiler;
    VM vm;
    Vector sources;
    
    initCompiler(&compiler, module);
    initVM(&vm, module);
    initVector(&sources);

    while (readSource(&compiler, &source, &size)) {
        appendReplHistory(&history, &historySize, &historyLength, source);
        validateReplHistory(history);
        pushVectorItem(&sources, source);
        source = NULL;
        size = 0;
        interpret(&vm);
    }

    freeVM(&vm);
    freeCompiler(&compiler);
    freeModuleObject(module);
    freeReplSources(&sources);
    free(source);
    free(history);
}
