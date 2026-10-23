#include "matchbox.h"
#include "compiler.h"
#include "program/buffer.h"
#include "runtime/module_object.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>

struct Matchbox
{
    ModuleObject* module;
    Compiler compiler;
    char* source;
};

static void resetMatchbox(Matchbox* mb)
{
    if (!mb->module) {
        return;
    }

    freeCompiler(&mb->compiler);
    freeModuleObject(mb->module);
    free(mb->source);

    mb->module = NULL;
    mb->source = NULL;
}

static void runModule(ModuleObject* module)
{
    VM vm;

    initVM(&vm, module);
    interpret(&vm);
    freeVM(&vm);
}

static bool runSource(Matchbox* mb, char* source)
{
    resetMatchbox(mb);

    mb->source = source;
    mb->module = createModuleObject();

    initCompiler(&mb->compiler, mb->module);

    bool compiled = compile(&mb->compiler, source);

    if (!compiled) {
        resetMatchbox(mb);

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

    size_t size = strlen(source) + 1;
    char* copy = malloc(size);

    if (!copy) {
        return false;
    }

    memcpy(copy, source, size);

    return runSource(mb, copy);
}
