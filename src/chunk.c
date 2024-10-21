#include "chunk.h"
#include <stdlib.h>
#include <stdio.h>

void initChunk(Chunk* chunk)
{
    chunk->capacity = CHUNK_INIT_CAPACITY;
    chunk->data = calloc(chunk->capacity, sizeof(uint8_t));
    chunk->count = 0;
    chunk->maxScopeLevel = 1;

    initFunctionArray(&chunk->functions);
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk)
{
    freeValueArray(&chunk->constants);
    freeFunctionArray(&chunk->functions);
    free(chunk->data);
}

size_t countChunk(Chunk* chunk)
{
    return chunk->count;
}

size_t getMaxScopeLevel(Chunk* chunk)
{
    return chunk->maxScopeLevel;
}

size_t setMaxScopeLevel(Chunk* chunk, size_t level)
{
    if (level > chunk->maxScopeLevel) {
        chunk->maxScopeLevel = level;
    }
}

void resizeChunk(Chunk* chunk, size_t capacity)
{
    chunk->data = realloc(chunk->data, sizeof(uint8_t) * capacity);
    chunk->capacity = capacity;
}

void pushByte(Chunk* chunk, uint8_t byte)
{
    if (chunk->capacity <= chunk->count) {
        resizeChunk(chunk, chunk->capacity * 2);
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
