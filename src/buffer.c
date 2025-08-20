#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
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

int getStreamContents(char **lineptr, size_t *n, FILE *stream)
{
    if (lineptr == NULL || n == NULL || stream == NULL) {
        return -1;
    }

    size_t len = 0;
    int c;

    if (*lineptr == NULL) {
        *n = 256;
        *lineptr = malloc(*n);

        if (*lineptr == NULL) {
            return -1;
        }
    }

    while ((c = fgetc(stream)) != EOF) {
        if (len + 1 >= *n) {
            *n *= 2;
            
            char *tmp = realloc(*lineptr, *n);

            if (tmp == NULL) {
                return -1;
            }

            *lineptr = tmp;
        }

        (*lineptr)[len++] = c;

        if (c == '\n') {
            break;
        }
    }

    if (len == 0 && c == EOF) {
        return -1;
    }

    (*lineptr)[len] = '\0';

    return len;
}
