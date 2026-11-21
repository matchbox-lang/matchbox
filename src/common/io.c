#include "common/io.h"
#include "common/errors.h"
#include "common/memory.h"
#include <stdio.h>
#include <string.h>

static char* read_stream(FILE* file)
{
    if (fseek(file, 0, SEEK_END) != 0) {
        return NULL;
    }

    long length = ftell(file);

    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        return NULL;
    }

    size_t size = (size_t)length;
    char* source = allocate_memory(size + 1);
    size_t count = fread(source, 1, size, file);

    if (count != size || ferror(file)) {
        free_memory(source);

        return NULL;
    }

    source[size] = '\0';

    return source;
}

char* read_file_contents(const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }

    ErrorContext context;

    begin_error_context(&context, NULL);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);
        fclose(file);
        raise_error();
    }

    char* source = read_stream(file);

    end_error_context(&context);

    int closed = fclose(file);

    if (closed != 0) {
        free_memory(source);

        return NULL;
    }

    return source;
}

size_t put_file_contents(const char* filename, const void* data)
{
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return 0;
    }

    size_t size = fwrite(data, 1, strlen(data), file);
    fclose(file);

    return size;
}

static void grow_stream_buffer(char** lineptr, size_t* capacity)
{
    size_t new_capacity = *capacity * 2;
    char* resized = reallocate_memory(*lineptr, new_capacity);
    *lineptr = resized;
    *capacity = new_capacity;
}

int read_stream_line(char **lineptr, size_t *n, FILE *stream)
{
    if (lineptr == NULL || n == NULL || stream == NULL) {
        return -1;
    }

    size_t length = 0;
    int c;

    if (*lineptr == NULL) {
        *n = 256;
        *lineptr = allocate_memory(*n);
    }

    while ((c = fgetc(stream)) != EOF) {
        if (length + 1 >= *n) {
            grow_stream_buffer(lineptr, n);
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
