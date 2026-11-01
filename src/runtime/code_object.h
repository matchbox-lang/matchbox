#ifndef CODE_OBJECT_H
#define CODE_OBJECT_H

#include "runtime/object.h"
#include <stddef.h>
#include <stdint.h>

#define AS_CODE_OBJECT(value) ((CodeObject*)AS_OBJECT(value))

typedef struct CodeObject
{
    Object obj;
    uint8_t* data;
    size_t capacity;
    size_t count;
} CodeObject;

void init_code_object(CodeObject* code);
void free_code_object(CodeObject* code);
void clear_code_object(CodeObject* code);
size_t count_code_object(const CodeObject* code);
void reserve_code_object(CodeObject* code, size_t capacity);
void resize_code_object(CodeObject* code, size_t size);
size_t push_byte(CodeObject* code, uint8_t byte);
void set_byte_at(CodeObject* code, size_t index, uint8_t byte);
uint8_t* code_object_begin(const CodeObject* code);
uint8_t* code_object_end(const CodeObject* code);

#endif
