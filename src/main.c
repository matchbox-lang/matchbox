#include "buffer.h"
#include "config.h"
#include "vm.h"
#include <stdlib.h>
#include <stdio.h>

static void repl(CommandArgs* args)
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
        
        interpret(data, args);
    }

    free(data);
}

static void file(CommandArgs* args)
{
    char* source = getFileContents(args->filename);

    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", args->filename);
        usage();
    }
    
    initVM();
    interpret(source, args);
    free(source);
}

int main(int argc, char* argv[])
{
    CommandArgs args;
    initCommandArgs(&args, argc, argv);

    if (argc == 1) {
        repl(&args);
    } else {
        file(&args);
    }

    return 0;
}
