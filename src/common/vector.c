#include "common/vector.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/util.h"
#include <stdint.h>
#include <stdlib.h>

void init_vector(Vector* vector)
{
    vector->data = NULL;
    vector->capacity = 0;
    vector->count = 0;
}

void free_vector(Vector* vector)
{
    free_memory(vector->data);
}

size_t count_vector(const Vector* vector)
{
    return vector->count;
}

void reserve_vector(Vector* vector, size_t capacity)
{
    if (capacity <= vector->capacity) {
        return;
    }

    if (capacity > SIZE_MAX / sizeof(void*)) {
        out_of_memory_error();
    }

    void** data = reallocate_memory(vector->data, sizeof(void*) * capacity);
    vector->data = data;
    vector->capacity = capacity;
}

void resize_vector(Vector* vector, size_t size)
{
    if (vector->capacity < size) {
        size_t capacity = MAX(vector->capacity * 2, size);

        reserve_vector(vector, capacity);
    }

    for (size_t i = vector->count; i < size; i++) {
        vector->data[i] = NULL;
    }

    vector->count = size;
}

size_t push_vector_item(Vector* vector, void* item)
{
    if (vector->capacity == vector->count) {
        size_t capacity = MAX(vector->capacity * 2, 4);

        reserve_vector(vector, capacity);
    }
    
    vector->data[vector->count++] = item;
    
    return vector->count;
}

void* pop_vector_item(Vector* vector)
{
    if (vector->count > 0) {
        return vector->data[--vector->count];
    }

    return NULL;
}

void* get_vector_at(const Vector* vector, size_t index)
{
    if (index >= vector->count) {
        return NULL;
    }

    return vector->data[index];
}

void set_vector_at(Vector* vector, size_t index, void* item)
{
    if (index < vector->count) {
        vector->data[index] = item;
    }
}

void* vector_begin(const Vector* vector)
{
    return vector->data;
}

void* vector_end(const Vector* vector)
{
    if (!vector->data) {
        return NULL;
    }

    return vector->data + vector->count;
}
