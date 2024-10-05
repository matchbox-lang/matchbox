#include "reference.h"
#include <stdlib.h>

void initReferenceArray(ReferenceArray* array)
{
    array->capacity = ARRAY_INIT_CAPACITY;
    array->data = malloc(array->capacity * sizeof(Reference));
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

void resizeReferenceArray(ReferenceArray* array, size_t capacity)
{
    array->data = realloc(array->data, sizeof(Reference) * capacity);
    array->capacity = capacity;
}

void pushReferenceArray(ReferenceArray* array, Reference ref)
{
    if (array->capacity == array->count) {
        resizeReferenceArray(array, array->capacity * 2);
    }
    
    array->data[array->count++] = ref;
}
