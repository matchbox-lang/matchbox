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
    int exit_status;
};

static void reset_matchbox(Matchbox* mb)
{
    free_memory_group(&mb->memory);

    mb->module = NULL;
}

static int run_module(ModuleObject* module)
{
    VM vm;
    init_vm(&vm, module);

    int status = interpret(&vm);

    free_vm(&vm);

    return status;
}

static bool run_source(Matchbox* mb, const char* source)
{
    size_t size = strlen(source) + 1;
    char* copy = allocate_memory(size);

    memcpy(copy, source, size);
    mb->module = create_module_object();
    init_compiler(&mb->compiler, mb->module);

    bool compiled = compile(&mb->compiler, copy);
    if (!compiled) {
        return false;
    }

    mb->exit_status = run_module(mb->module);

    return true;
}

int matchbox_exit_status(const Matchbox* mb)
{
    if (!mb) {
        return EXIT_SUCCESS;
    }

    return mb->exit_status;
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

    reset_matchbox(mb);
    free(mb);
}

bool matchbox_run(Matchbox* mb, const char* source)
{
    if (!mb || !source) {
        return false;
    }

    ErrorContext context;

    mb->exit_status = EXIT_SUCCESS;

    reset_matchbox(mb);
    begin_error_context(&context, &mb->memory);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);
        reset_matchbox(mb);

        return false;
    }

    bool success = run_source(mb, source);

    end_error_context(&context);

    if (!success) {
        reset_matchbox(mb);
    }

    return success;
}
