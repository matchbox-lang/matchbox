#include "repl.h"
#include "buffer.h"
#include "compiler.h"
#include "module_object.h"
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

    if (!read) {
        printf("\n");
    }

    return read;
}

void runRepl()
{
    char* source = NULL;
    size_t size = 0;
    ModuleObject* module = createModuleObject();
    Compiler compiler;
    VM vm;
    
    initCompiler(&compiler, module);
    initVM(&vm, module);

    while (readSource(&compiler, &source, &size)) {
        interpret(&vm);
    }

    freeVM(&vm);
    freeCompiler(&compiler);
    freeModuleObject(module);
    free(source);
}
