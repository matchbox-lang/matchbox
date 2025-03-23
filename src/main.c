#include "buffer.h"
#include "config.h"
#include "vm.h"
#include <stdlib.h>
#include <stdio.h>

CommandArgs cargs;

static void repl()
{
    char* data = NULL;
    size_t size = 0;
    size_t len;
    
    initVM();

    while (1) {
        printf(">>> ");

        len = getStreamContents(&data, &size, stdin);

        if (len == -1) {
            printf("\n");
            break;
        }
        
        interpret(data);
    }

    free(data);
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
    initCommandArgs(&cargs, argc, argv);

    if (argc == 1) {
        repl();
    } else {
        file();
    }

    return 0;
}
