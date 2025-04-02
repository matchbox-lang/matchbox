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
    ModuleObject* module = createModuleObject();
    
    initVM();

    while (1) {
        printf(">>> ");

        len = getStreamContents(&source, &size, stdin);

        if (len == -1) {
            printf("\n");
            break;
        }
        
        compile(source, module);
        interpret(module);
    }

    freeModuleObject(module);
    freeVM();
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
    compile(source, module);

    if (cargs.disassemble) {
        return disassembleModule(module);
    }

    initVM();
    interpret(module);
    freeModuleObject(module);
    freeVM();
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
