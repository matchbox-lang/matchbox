#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

typedef struct Vector
{
    void** data;
    size_t capacity;
    size_t count;
} Vector;

void initVector(Vector* vector);
void freeVector(Vector* vector);
size_t countVector(Vector* vector);
void reserveVector(Vector* vector, size_t capacity);
void resizeVector(Vector* vector, size_t size);
size_t pushVectorItem(Vector* vector, void* item);
void* popVectorItem(Vector* vector);
void* getVectorAt(Vector* vector, size_t index);
void setVectorAt(Vector* vector, size_t index, void* item);
void* vectorBegin(Vector* vector);
void* vectorEnd(Vector* vector);

#endif
