#include "common/memory.h"
#include "common/errors.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

union Allocation
{
    max_align_t alignment;
    struct
    {
        MemoryGroup* group;
        Allocation* previous;
        Allocation* next;
    } links;
};

static _Thread_local MemoryGroup* currentGroup;

MemoryGroup* useMemoryGroup(MemoryGroup* group)
{
    MemoryGroup* previous = currentGroup;
    currentGroup = group;

    return previous;
}

static void linkAllocation(Allocation* allocation)
{
    MemoryGroup* group = allocation->links.group;

    if (!group) {
        return;
    }

    if (allocation->links.previous) {
        allocation->links.previous->links.next = allocation;
    } else {
        group->allocations = allocation;
    }

    if (allocation->links.next) {
        allocation->links.next->links.previous = allocation;
    }
}

static void unlinkAllocation(Allocation* allocation)
{
    MemoryGroup* group = allocation->links.group;

    if (!group) {
        return;
    }

    if (allocation->links.previous) {
        allocation->links.previous->links.next = allocation->links.next;
    } else {
        group->allocations = allocation->links.next;
    }

    if (allocation->links.next) {
        allocation->links.next->links.previous = allocation->links.previous;
    }
}

void* allocateMemory(size_t size)
{
    if (size > SIZE_MAX - sizeof(Allocation)) {
        outOfMemoryError();
    }

    Allocation* allocation = malloc(sizeof(Allocation) + size);

    if (!allocation) {
        outOfMemoryError();
    }

    allocation->links.group = currentGroup;
    allocation->links.previous = NULL;
    allocation->links.next = NULL;

    if (currentGroup) {
        allocation->links.next = currentGroup->allocations;
    }

    linkAllocation(allocation);

    return allocation + 1;
}

void* allocateZeroedMemory(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) {
        outOfMemoryError();
    }

    size_t length = count * size;
    void* pointer = allocateMemory(length);

    memset(pointer, 0, length);

    return pointer;
}

void* reallocateMemory(void* pointer, size_t size)
{
    if (!pointer) {
        return allocateMemory(size);
    }

    if (size > SIZE_MAX - sizeof(Allocation)) {
        outOfMemoryError();
    }

    Allocation* original = (Allocation*)pointer - 1;
    Allocation* allocation = realloc(original, sizeof(Allocation) + size);

    if (!allocation) {
        outOfMemoryError();
    }

    linkAllocation(allocation);

    return allocation + 1;
}

void freeMemory(void* pointer)
{
    if (!pointer) {
        return;
    }

    Allocation* allocation = (Allocation*)pointer - 1;

    unlinkAllocation(allocation);
    free(allocation);
}

void freeMemoryGroup(MemoryGroup* group)
{
    while (group->allocations) {
        void* pointer = group->allocations + 1;

        freeMemory(pointer);
    }
}
