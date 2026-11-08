#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

typedef union Allocation Allocation;

typedef struct MemoryGroup
{
    Allocation* allocations;
} MemoryGroup;

MemoryGroup* use_memory_group(MemoryGroup* group);
void free_memory_group(MemoryGroup* group);
void* allocate_memory(size_t size);
void* allocate_zeroed_memory(size_t count, size_t size);
void* reallocate_memory(void* pointer, size_t size);
void free_memory(void* pointer);

#endif
