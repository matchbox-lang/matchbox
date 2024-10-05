#include "vector.h"
#include <stdlib.h>

void initVector(Vector* vector)
{
    vector->capacity = VECTOR_INIT_CAPACITY;
    vector->data = malloc(vector->capacity * sizeof(void*));
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

void resizeVector(Vector* vector, size_t capacity)
{
    vector->data = realloc(vector->data, sizeof(void*) * capacity);
    vector->capacity = capacity;
}

void pushVector(Vector* vector, void* item)
{
    if (vector->capacity == vector->count) {
        resizeVector(vector, vector->capacity * 2);
    }
    
    vector->data[vector->count++] = item;
}

void* popVector(Vector* vector)
{
    if (vector->count > 0) {
        return vector->data[vector->count--];
    }

    return NULL;
}

void* getVectorAt(Vector* vector, size_t index)
{
    if (index >= 0 && index < vector->count) {
        return vector->data[index];
    }

    return NULL;
}

void setVectorAt(Vector* vector, size_t index, void* item)
{
    if (index >= 0 && index < vector->count) {
        vector->data[index] = item;
    }
}
