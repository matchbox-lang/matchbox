#include "vector.h"
#include <stdlib.h>

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity * 2)

void initVector(Vector* vector)
{
    vector->data = NULL;
    vector->capacity = 0;
    vector->count = 0;
}

void freeVector(Vector* vector)
{
    free(vector->data);
}

size_t countVector(Vector* vector)
{
    return vector->count;
}

void reserveVector(Vector* vector, size_t capacity)
{
    vector->data = realloc(vector->data, sizeof(void*) * capacity);
    vector->capacity = capacity;
}

void pushVectorItem(Vector* vector, void* item)
{
    if (vector->capacity == vector->count) {
        reserveVector(vector, GROW_CAPACITY(vector->capacity));
    }
    
    vector->data[vector->count++] = item;
}

void* popVectorItem(Vector* vector)
{
    if (vector->count > 0) {
        return vector->data[vector->count--];
    }

    return NULL;
}

void* getVectorAt(Vector* vector, size_t index)
{
    return vector->data[index];
}

void setVectorAt(Vector* vector, size_t index, void* item)
{
    if (index >= 0 && index < vector->count) {
        vector->data[index] = item;
    }
}

void* vectorBegin(Vector* vector)
{
    return vector->data;
}

void* vectorEnd(Vector* vector)
{
    return vector->data + vector->count;
}
