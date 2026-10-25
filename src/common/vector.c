#include "common/vector.h"
#include "common/memory.h"
#include "common/errors.h"
#include "common/util.h"
#include <stdint.h>
#include <stdlib.h>

void initVector(Vector* vector)
{
    vector->data = NULL;
    vector->capacity = 0;
    vector->count = 0;
}

void freeVector(Vector* vector)
{
    freeMemory(vector->data);
}

size_t countVector(Vector* vector)
{
    return vector->count;
}

void reserveVector(Vector* vector, size_t capacity)
{
    if (capacity <= vector->capacity) {
        return;
    }

    if (capacity > SIZE_MAX / sizeof(void*)) {
        outOfMemoryError();
    }

    void** data = reallocateMemory(vector->data, sizeof(void*) * capacity);
    vector->data = data;
    vector->capacity = capacity;
}

void resizeVector(Vector* vector, size_t size)
{
    if (vector->capacity < size) {
        size_t capacity = MAX(vector->capacity * 2, size);

        reserveVector(vector, capacity);
    }

    for (size_t i = vector->count; i < size; i++) {
        vector->data[i] = NULL;
    }

    vector->count = size;
}

size_t pushVectorItem(Vector* vector, void* item)
{
    if (vector->capacity == vector->count) {
        size_t capacity = MAX(vector->capacity * 2, 4);

        reserveVector(vector, capacity);
    }
    
    vector->data[vector->count++] = item;
    
    return vector->count;
}

void* popVectorItem(Vector* vector)
{
    if (vector->count > 0) {
        return vector->data[--vector->count];
    }

    return NULL;
}

void* getVectorAt(Vector* vector, size_t index)
{
    if (index >= vector->count) {
        return NULL;
    }

    return vector->data[index];
}

void setVectorAt(Vector* vector, size_t index, void* item)
{
    if (index < vector->count) {
        vector->data[index] = item;
    }
}

void* vectorBegin(Vector* vector)
{
    return vector->data;
}

void* vectorEnd(Vector* vector)
{
    if (!vector->data) {
        return NULL;
    }

    return vector->data + vector->count;
}
