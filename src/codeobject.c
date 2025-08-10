#include "codeobject.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity * 2)

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
    code->data = realloc(code->data, sizeof(uint8_t) * capacity);
    code->capacity = capacity;
}

void resizeCodeObject(CodeObject* code, size_t size)
{
    reserveCodeObject(code, size);
    code->count = size;
}

size_t pushByte(CodeObject* code, uint8_t byte)
{
    if (code->capacity <= code->count) {
        reserveCodeObject(code, GROW_CAPACITY(code->capacity));
    }

    code->data[code->count] = byte;
    return ++code->count;
}

void setByteAt(CodeObject* code, size_t index, uint8_t byte)
{
    if (index >= 0 && index < code->count) {
        code->data[index] = byte;
    }
}

uint8_t* codeObjectBegin(CodeObject* code)
{
    return code->data;
}

uint8_t* codeObjectEnd(CodeObject* code)
{
    return code->data + code->count;
}
