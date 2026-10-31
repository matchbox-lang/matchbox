#include "common/io.h"
#include "common/memory.h"
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
    char* source = allocateMemory(size + 1);
    size_t count = fread(source, 1, size, file);

    if (count != size || ferror(file)) {
        freeMemory(source);

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
        freeMemory(source);

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

static void growStreamBuffer(char** lineptr, size_t* capacity)
{
    size_t newCapacity = *capacity * 2;
    char* resized = reallocateMemory(*lineptr, newCapacity);
    *lineptr = resized;
    *capacity = newCapacity;
}

int readStreamLine(char **lineptr, size_t *n, FILE *stream)
{
    if (lineptr == NULL || n == NULL || stream == NULL) {
        return -1;
    }

    size_t length = 0;
    int c;

    if (*lineptr == NULL) {
        *n = 256;
        *lineptr = allocateMemory(*n);
    }

    while ((c = fgetc(stream)) != EOF) {
        if (length + 1 >= *n) {
            growStreamBuffer(lineptr, n);
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
