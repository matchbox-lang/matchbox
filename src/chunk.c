#include "chunk.h"
#include <stdlib.h>

#define GROW_CAPACITY(capacity) (capacity < 8 ? 8 : capacity * 2)

void initChunk(Chunk* chunk)
{
    chunk->data = NULL;
    chunk->capacity = 0;
    chunk->count = 0;

    initFunctionArray(&chunk->functions);
    initValueArray(&chunk->globals);
}

void freeChunk(Chunk* chunk)
{
    freeValueArray(&chunk->globals);
    freeFunctionArray(&chunk->functions);
    free(chunk->data);
}

size_t countChunk(Chunk* chunk)
{
    return chunk->count;
}

void reserveChunk(Chunk* chunk, size_t capacity)
{
    chunk->data = realloc(chunk->data, sizeof(uint8_t) * capacity);
    chunk->capacity = capacity;
}

void pushByte(Chunk* chunk, uint8_t byte)
{
    if (chunk->capacity <= chunk->count) {
        reserveChunk(chunk, GROW_CAPACITY(chunk->capacity));
    }

    chunk->data[chunk->count] = byte;
    chunk->count++;
}

uint8_t getByteAt(Chunk* chunk, size_t index)
{
    return chunk->data[index];
}

void setByteAt(Chunk* chunk, size_t index, uint8_t byte)
{
    if (index >= 0 && index < chunk->count) {
        chunk->data[index] = byte;
    }
}

uint8_t* chunkBegin(Chunk* chunk)
{
    return chunk->data;
}

uint8_t* chunkEnd(Chunk* chunk)
{
    return chunk->data + chunk->count;
}
