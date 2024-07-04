#include "file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char* getFileContents(const char* filename)
{
    FILE* fp = fopen(filename, "rb");

    if (!fp) {
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* data = malloc(sizeof(char) * (length + 1));
    
    if (!data) {
        return NULL;
    }
    
    fread(data, sizeof(char), length, fp);
    data[length] = '\0';
    fclose(fp);

    return data;
}

void freeFileContents(void* data)
{
    free(data);
}

size_t putFileContents(const char* filename, const void* data)
{
    FILE* fp = fopen(filename, "wb");

    if (!fp) {
        return 0;
    }

    size_t size = fwrite(data, sizeof(char), strlen(data), fp);
    fclose(fp);

    return size;
}
