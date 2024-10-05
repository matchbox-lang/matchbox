#ifndef REFERENCE_H
#define REFERENCE_H

#include "ast.h"

#define ARRAY_INIT_CAPACITY 4

typedef struct Reference
{
    AST* ast;
    size_t position;
} Reference;

typedef struct ReferenceArray
{
    Reference* data;
    size_t capacity;
    size_t count;
} ReferenceArray;

void initReferenceArray(ReferenceArray* array);
void freeReferenceArray(ReferenceArray* array);
size_t countReferenceArray(ReferenceArray* array);
void resizeReferenceArray(ReferenceArray* array, size_t capacity);
void pushReferenceArray(ReferenceArray* array, Reference ref);
Reference getReferenceAt(ReferenceArray* array, size_t index);
void setReferenceAt(ReferenceArray* array, size_t index, Reference item);

#endif
