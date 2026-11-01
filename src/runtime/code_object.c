#include "runtime/code_object.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/util.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void init_code_object(CodeObject* code)
{
    code->data = NULL;
    code->capacity = 0;
    code->count = 0;
}

void free_code_object(CodeObject* code)
{
    free_memory(code->data);
}

void clear_code_object(CodeObject* code)
{
    code->count = 0;
}

size_t count_code_object(const CodeObject* code)
{
    return code->count;
}

void reserve_code_object(CodeObject* code, size_t capacity)
{
    if (capacity <= code->capacity) {
        return;
    }

    if (capacity > SIZE_MAX / sizeof(uint8_t)) {
        out_of_memory_error();
    }

    uint8_t* data = reallocate_memory(code->data, sizeof(uint8_t) * capacity);
    code->data = data;
    code->capacity = capacity;
}

void resize_code_object(CodeObject* code, size_t size)
{
    if (code->capacity < size) {
        size_t capacity = MAX(code->capacity * 2, size);

        reserve_code_object(code, capacity);
    }

    for (size_t i = code->count; i < size; i++) {
        code->data[i] = 0;
    }

    code->count = size;
}

size_t push_byte(CodeObject* code, uint8_t byte)
{
    if (code->capacity <= code->count) {
        size_t capacity = MAX(code->capacity * 2, 4);

        reserve_code_object(code, capacity);
    }

    code->data[code->count] = byte;

    return ++code->count;
}

void set_byte_at(CodeObject* code, size_t index, uint8_t byte)
{
    if (index < code->count) {
        code->data[index] = byte;
    }
}

uint8_t* code_object_begin(const CodeObject* code)
{
    return code->data;
}

uint8_t* code_object_end(const CodeObject* code)
{
    if (!code->data) {
        return NULL;
    }

    return code->data + code->count;
}
