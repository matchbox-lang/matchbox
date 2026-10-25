#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

typedef union Allocation Allocation;

typedef struct MemoryGroup
{
    Allocation* allocations;
} MemoryGroup;

MemoryGroup* useMemoryGroup(MemoryGroup* group);
void freeMemoryGroup(MemoryGroup* group);
void* allocateMemory(size_t size);
void* allocateZeroedMemory(size_t count, size_t size);
void* reallocateMemory(void* pointer, size_t size);
void freeMemory(void* pointer);

#endif
