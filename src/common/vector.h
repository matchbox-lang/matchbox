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
size_t countVector(const Vector* vector);
void reserveVector(Vector* vector, size_t capacity);
void resizeVector(Vector* vector, size_t size);
size_t pushVectorItem(Vector* vector, void* item);
void* popVectorItem(Vector* vector);
void* getVectorAt(const Vector* vector, size_t index);
void setVectorAt(Vector* vector, size_t index, void* item);
void* vectorBegin(const Vector* vector);
void* vectorEnd(const Vector* vector);

#endif
