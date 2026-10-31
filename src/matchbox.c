#include "matchbox.h"
#include "common/errors.h"
#include "common/memory.h"
#include "compiler/compiler.h"
#include "runtime/module_object.h"
#include "vm/vm.h"
#include <stdlib.h>
#include <string.h>

struct Matchbox
{
    MemoryGroup memory;
    ModuleObject* module;
    Compiler compiler;
    int exitStatus;
};

static void resetMatchbox(Matchbox* mb)
{
    freeMemoryGroup(&mb->memory);

    mb->module = NULL;
}

static int runModule(ModuleObject* module)
{
    VM vm;

    initVM(&vm, module);

    int status = interpret(&vm);

    freeVM(&vm);

    return status;
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

    mb->exitStatus = runModule(mb->module);

    return true;
}

int matchbox_exit_status(const Matchbox* mb)
{
    if (!mb) {
        return EXIT_SUCCESS;
    }

    return mb->exitStatus;
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

    mb->exitStatus = EXIT_SUCCESS;

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
