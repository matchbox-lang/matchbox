#include "buffer.h"
#include "command_line.h"
#include "compiler.h"
#include "module_object.h"
#include "options.h"
#include "program.h"
#include "vm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static void repl()
{
    char* source = NULL;
    size_t size = 0;
    size_t len;
    ModuleObject* module = createModuleObject();
    Compiler compiler;
    VM vm;
    
    initCompiler(&compiler, module);
    initVM(&vm, module);

    while (1) {
        printf(">>> ");

        len = getStreamContents(&source, &size, stdin);

        if (len == -1) {
            printf("\n");
            break;
        }

        compile(&compiler, source);
        interpret(&vm);
    }

    freeVM(&vm);
    freeCompiler(&compiler);
    freeModuleObject(module);
    free(source);
}

static void runFile(Options* options)
{
    char* source = getFileContents(options->filename);

    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", options->filename);
        exit(1);
    }

    ModuleObject* module = createModuleObject();
    Compiler compiler;
    VM vm;
    bool vmInitialized = false;
    
    initCompiler(&compiler, module);
    compile(&compiler, source);

    if (options->disassemble) {
        disassembleModule(module);
    } else {
        initVM(&vm, module);
        vmInitialized = true;
        interpret(&vm);
    }

    if (vmInitialized) {
        freeVM(&vm);
    }

    freeCompiler(&compiler);
    freeModuleObject(module);
    free(source);
}

int main(int argc, char* argv[])
{
    Options options;

    initOptions(&options);

    if (!parseCommandLine(&options, argc, argv)) {
        printUsage(stderr);

        return 1;
    }

    if (!options.filename) {
        repl();
    } else {
        runFile(&options);
    }

    return 0;
}
