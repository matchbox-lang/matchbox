#ifndef REFERENCE_H
#define REFERENCE_H

#include "ast.h"

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
void reserveReferenceArray(ReferenceArray* array, size_t capacity);
void pushReference(ReferenceArray* array, Reference ref);
Reference getReferenceAt(ReferenceArray* array, size_t index);
void setReferenceAt(ReferenceArray* array, size_t index, Reference item);

#endif
