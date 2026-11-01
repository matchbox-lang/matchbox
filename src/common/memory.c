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

static _Thread_local MemoryGroup* current_group;

MemoryGroup* use_memory_group(MemoryGroup* group)
{
    MemoryGroup* previous = current_group;
    current_group = group;

    return previous;
}

static void link_allocation(Allocation* allocation)
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

static void unlink_allocation(Allocation* allocation)
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

void* allocate_memory(size_t size)
{
    if (size > SIZE_MAX - sizeof(Allocation)) {
        out_of_memory_error();
    }

    Allocation* allocation = malloc(sizeof(Allocation) + size);

    if (!allocation) {
        out_of_memory_error();
    }

    allocation->links.group = current_group;
    allocation->links.previous = NULL;
    allocation->links.next = NULL;

    if (current_group) {
        allocation->links.next = current_group->allocations;
    }

    link_allocation(allocation);

    return allocation + 1;
}

void* allocate_zeroed_memory(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) {
        out_of_memory_error();
    }

    size_t length = count * size;
    void* pointer = allocate_memory(length);

    memset(pointer, 0, length);

    return pointer;
}

void* reallocate_memory(void* pointer, size_t size)
{
    if (!pointer) {
        return allocate_memory(size);
    }

    if (size > SIZE_MAX - sizeof(Allocation)) {
        out_of_memory_error();
    }

    Allocation* original = (Allocation*)pointer - 1;
    Allocation* allocation = realloc(original, sizeof(Allocation) + size);

    if (!allocation) {
        out_of_memory_error();
    }

    link_allocation(allocation);

    return allocation + 1;
}

void free_memory(void* pointer)
{
    if (!pointer) {
        return;
    }

    Allocation* allocation = (Allocation*)pointer - 1;

    unlink_allocation(allocation);
    free(allocation);
}

void free_memory_group(MemoryGroup* group)
{
    while (group->allocations) {
        void* pointer = group->allocations + 1;

        free_memory(pointer);
    }
}
