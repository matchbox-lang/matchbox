#include "matchbox.h"
#include "common/errors.h"
#include "common/memory.h"
#include "compiler.h"
#include "runtime/module_object.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>

struct Matchbox
{
    MemoryGroup memory;
    ModuleObject* module;
    Compiler compiler;
};

static void resetMatchbox(Matchbox* mb)
{
    freeMemoryGroup(&mb->memory);

    mb->module = NULL;
}

static void runModule(ModuleObject* module)
{
    VM vm;

    initVM(&vm, module);
    interpret(&vm);
    freeVM(&vm);
}

static bool runSource(Matchbox* mb, const char* source)
{
    size_t size = strlen(source) + 1;
    char* copy = allocateMemory(size);

    memcpy(copy, source, size);
    mb->module = createModuleObject();
    initCompiler(&mb->compiler, mb->module);

    bool compiled = compile(&mb->compiler, copy);
    if (!compiled) {
        return false;
    }

    runModule(mb->module);

    return true;
}

Matchbox* matchbox_create(void)
{
    Matchbox* mb = calloc(1, sizeof(Matchbox));

    return mb;
}

void matchbox_destroy(Matchbox* mb)
{
    if (!mb) {
        return;
    }

    resetMatchbox(mb);
    free(mb);
}

bool matchbox_run(Matchbox* mb, const char* source)
{
    if (!mb || !source) {
        return false;
    }

    ErrorContext context;

    resetMatchbox(mb);
    beginErrorContext(&context, &mb->memory);

    if (setjmp(context.jump) != 0) {
        endErrorContext(&context);
        resetMatchbox(mb);

        return false;
    }

    bool success = runSource(mb, source);

    endErrorContext(&context);

    if (!success) {
        resetMatchbox(mb);
    }

    return success;
}
