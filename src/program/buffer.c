#include "program/buffer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* readFileContents(FILE* file)
{
    if (fseek(file, 0, SEEK_END) != 0) {
        return NULL;
    }

    long length = ftell(file);

    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        return NULL;
    }

    size_t size = (size_t)length;
    char* source = malloc(size + 1);

    if (!source) {
        return NULL;
    }

    size_t count = fread(source, 1, size, file);

    if (count != size || ferror(file)) {
        free(source);

        return NULL;
    }

    source[size] = '\0';

    return source;
}

char* getFileContents(const char* filename)
{
    FILE* file = fopen(filename, "rb");

    if (!file) {
        return NULL;
    }

    char* source = readFileContents(file);
    int closed = fclose(file);

    if (closed != 0) {
        free(source);

        return NULL;
    }

    return source;
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

static bool growStreamBuffer(char** lineptr, size_t* capacity)
{
    size_t newCapacity = *capacity * 2;
    char* resized = realloc(*lineptr, newCapacity);

    if (!resized) {
        return false;
    }

    *lineptr = resized;
    *capacity = newCapacity;

    return true;
}

int getStreamContents(char **lineptr, size_t *n, FILE *stream)
{
    if (lineptr == NULL || n == NULL || stream == NULL) {
        return -1;
    }

    size_t length = 0;
    int c;

    if (*lineptr == NULL) {
        *n = 256;
        *lineptr = malloc(*n);
    }

    if (*lineptr == NULL) {
        return -1;
    }

    while ((c = fgetc(stream)) != EOF) {
        if (length + 1 >= *n && !growStreamBuffer(lineptr, n)) {
            return -1;
        }

        (*lineptr)[length++] = c;

        if (c == '\n') {
            break;
        }
    }

    if (length == 0 && c == EOF) {
        return -1;
    }

    (*lineptr)[length] = '\0';

    return length;
}
