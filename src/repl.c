#include "repl.h"
#include "buffer.h"
#include "compiler.h"
#include "module_object.h"
#include "vm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static bool readReplSource(char** source, size_t* size)
{
    printf(">>> ");

    if (getStreamContents(source, size, stdin) == -1) {
        printf("\n");
        
        return false;
    }

    return true;
}

static void runReplSource(Compiler* compiler, VM* vm, char* source)
{
    compileRepl(compiler, source);
    interpret(vm);
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

    while (readReplSource(&source, &size)) {
        runReplSource(&compiler, &vm, source);
    }

    freeVM(&vm);
    freeCompiler(&compiler);
    freeModuleObject(module);
    free(source);
}
