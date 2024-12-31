#include "reference.h"
#include <stdlib.h>

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity * 2)

void initReferenceArray(ReferenceArray* array)
{
    array->data = NULL;
    array->capacity = 0;
    array->count = 0;
}

void freeReferenceArray(ReferenceArray* array)
{
    free(array->data);
}

size_t countReferenceArray(ReferenceArray* array)
{
    return array->count;
}

void reserveReferenceArray(ReferenceArray* array, size_t capacity)
{
    array->data = realloc(array->data, sizeof(Reference) * capacity);
    array->capacity = capacity;
}

size_t pushReference(ReferenceArray* array, Reference ref)
{
    if (array->capacity == array->count) {
        reserveReferenceArray(array, GROW_CAPACITY(array->capacity));
    }
    
    array->data[array->count++] = ref;
    return array->count;
}

Reference getReferenceAt(ReferenceArray* array, size_t index)
{
    return array->data[index];
}

void setReferenceAt(ReferenceArray* array, size_t index, Reference item)
{
    if (index >= 0 && index < array->count) {
        array->data[index] = item;
    }
}
