#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

typedef struct Vector
{
    void** data;
    size_t capacity;
    size_t count;
} Vector;

void init_vector(Vector* vector);
void free_vector(Vector* vector);
size_t count_vector(const Vector* vector);
void reserve_vector(Vector* vector, size_t capacity);
void resize_vector(Vector* vector, size_t size);
size_t push_vector_item(Vector* vector, void* item);
void* pop_vector_item(Vector* vector);
void* get_vector_at(const Vector* vector, size_t index);
void set_vector_at(Vector* vector, size_t index, void* item);
void* vector_begin(const Vector* vector);
void* vector_end(const Vector* vector);

#endif
