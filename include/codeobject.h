#ifndef CODE_OBJECT_H
#define CODE_OBJECT_H

#include "object.h"
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

void initCodeObject(CodeObject* code);
void freeCodeObject(CodeObject* code);
size_t countCodeObject(CodeObject* code);
void reserveCodeObject(CodeObject* code, size_t capacity);
size_t pushByte(CodeObject* code, uint8_t byte);
void setByteAt(CodeObject* code, size_t index, uint8_t byte);
uint8_t* codeBegin(CodeObject* code);
uint8_t* codeEnd(CodeObject* code);

#endif
