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
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* data = malloc(len + 1);
    
    if (!data) {
        return NULL;
    }
    
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);

    return data;
}

size_t putFileContents(const char* filename, const void* data)
{
    FILE* fp = fopen(filename, "wb");

    if (!fp) {
        return 0;
    }

    size_t size = fwrite(data, 1, strlen(data), fp);
    fclose(fp);

    return size;
}
