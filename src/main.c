#include "buffer.h"
#include "compiler.h"
#include "opcode.h"
#include "program.h"
#include "functionobject.h"
#include "moduleobject.h"
#include "vm.h"
#include <stdlib.h>

ProgramOptions options;

static void repl()
{
    char* source = NULL;
    size_t size = 0;
    size_t len;
    ModuleObject* module = createModuleObject();
    VM vm;
    
    initCompiler(module);
    initVM(&vm, module);

    while (1) {
        printf(">>> ");

        len = getStreamContents(&source, &size, stdin);

        if (len == -1) {
            printf("\n");
            break;
        }

        compile(source);
        interpret(&vm);
    }

    freeVM(&vm);
    freeCompiler();
    freeModuleObject(module);
    free(source);
}

static void file()
{
    char* source = getFileContents(options.filename);

    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", options.filename);
        printUsage();
    }

    ModuleObject* module = createModuleObject();
    VM vm;
    
    initCompiler(module);
    compile(source);

    if (options.disassemble) {
        return disassembleModule(module);
    }

    initVM(&vm, module);
    interpret(&vm);
    freeVM(&vm);
    freeCompiler();
    freeModuleObject(module);
    free(source);
}

int main(int argc, char* argv[])
{
    initProgramOptions(&options, argc, argv);

    if (argc == 1) {
        repl();
    } else {
        file();
    }

    return 0;
}
