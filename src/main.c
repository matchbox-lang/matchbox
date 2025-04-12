#include "buffer.h"
#include "bytecode.h"
#include "config.h"
#include "compiler.h"
#include "functionobject.h"
#include "moduleobject.h"
#include "vm.h"
#include <stdlib.h>

CommandArguments cargs;

static void repl()
{
    char* source = NULL;
    size_t size = 0;
    size_t len;

    while (1) {
        printf(">>> ");

        len = getStreamContents(&source, &size, stdin);

        if (len == -1) {
            printf("\n");
            break;
        }

        ModuleObject* module = createModuleObject();
        
        initCompiler(module);
        compile(source);

        initVM(module);
        interpret();
        freeVM();
        freeModuleObject(module);
        freeCompiler();
    }

    free(source);
}

static void file()
{
    char* source = getFileContents(cargs.filename);

    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", cargs.filename);
        printUsage();
    }

    ModuleObject* module = createModuleObject();
    
    initCompiler(module);
    compile(source);

    if (cargs.disassemble) {
        return disassembleModule(module);
    }

    initVM(module);
    interpret();
    freeVM();
    freeModuleObject(module);
    freeCompiler();
    free(source);
}

int main(int argc, char* argv[])
{
    initCommandArguments(&cargs, argc, argv);

    if (argc == 1) {
        repl();
    } else {
        file();
    }

    return 0;
}
