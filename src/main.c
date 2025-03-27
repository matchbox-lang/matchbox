#include "buffer.h"
#include "chunk.h"
#include "compiler.h"
#include "config.h"
#include "parser.h"
#include "scope.h"
#include "vm.h"
#include <stdlib.h>
#include <stdio.h>

CommandArguments cargs;
ValueArray globals;

static void repl()
{
    char* source = NULL;
    size_t size = 0;
    size_t len;
    
    initVM();

    while (1) {
        printf(">>> ");

        len = getStreamContents(&source, &size, stdin);

        if (len == -1) {
            printf("\n");
            break;
        }
        
        interpret(source);
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
    
    initVM();
    interpret(source);
    free(source);
}

int main(int argc, char* argv[])
{
    initCommandArguments(&cargs, argc, argv);
    initValueArray(&globals);

    if (argc == 1) {
        repl();
    } else {
        file();
    }

    freeValueArray(&globals);

    return 0;
}
