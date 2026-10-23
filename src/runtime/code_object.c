#include "runtime/code_object.h"
#include "common/util.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void initCodeObject(CodeObject* code)
{
    code->data = NULL;
    code->capacity = 0;
    code->count = 0;
}

void freeCodeObject(CodeObject* code)
{
    free(code->data);
}

void clearCodeObject(CodeObject* code)
{
    code->count = 0;
}

size_t countCodeObject(CodeObject* code)
{
    return code->count;
}

void reserveCodeObject(CodeObject* code, size_t capacity)
{
    if (capacity <= code->capacity) {
        return;
    }

    if (capacity > SIZE_MAX / sizeof(uint8_t)) {
        outOfMemoryError();
    }

    uint8_t* data = realloc(code->data, sizeof(uint8_t) * capacity);
    if (!data) {
        outOfMemoryError();
    }

    code->data = data;
    code->capacity = capacity;
}

void resizeCodeObject(CodeObject* code, size_t size)
{
    if (code->capacity < size) {
        size_t capacity = MAX(code->capacity * 2, size);

        reserveCodeObject(code, capacity);
    }

    for (size_t i = code->count; i < size; i++) {
        code->data[i] = 0;
    }

    code->count = size;
}

size_t pushByte(CodeObject* code, uint8_t byte)
{
    if (code->capacity <= code->count) {
        size_t capacity = MAX(code->capacity * 2, 4);

        reserveCodeObject(code, capacity);
    }

    code->data[code->count] = byte;

    return ++code->count;
}

void setByteAt(CodeObject* code, size_t index, uint8_t byte)
{
    if (index < code->count) {
        code->data[index] = byte;
    }
}

uint8_t* codeObjectBegin(CodeObject* code)
{
    return code->data;
}

uint8_t* codeObjectEnd(CodeObject* code)
{
    if (!code->data) {
        return NULL;
    }

    return code->data + code->count;
}
