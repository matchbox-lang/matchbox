#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

#define VECTOR_INIT_CAPACITY 4

typedef struct Vector
{
    void** data;
    size_t capacity;
    size_t count;
} Vector;

void initVector(Vector* vector);
void freeVector(Vector* vector);
size_t countVector(Vector* vector);
void resizeVector(Vector* vector, size_t capacity);
void pushVector(Vector* vector, void* item);
void* popVector(Vector* vector);
void* vectorGet(Vector* vector, size_t index);
void vectorSet(Vector* vector, size_t index, void* item);

#endif
