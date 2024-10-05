#include "chunk.h"
#include <stdlib.h>
#include <stdio.h>

void initChunk(Chunk* chunk)
{
    chunk->capacity = CHUNK_INIT_CAPACITY;
    chunk->data = calloc(chunk->capacity, sizeof(uint8_t));
    chunk->count = 0;

    initFunctionArray(&chunk->functions);
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk)
{
    freeFunctionArray(&chunk->functions);
    freeValueArray(&chunk->constants);

    free(chunk->data);
}

size_t countChunk(Chunk* chunk)
{
    return chunk->count;
}

void resizeChunk(Chunk* chunk, size_t capacity)
{
    chunk->data = realloc(chunk->data, sizeof(uint8_t) * capacity);
    chunk->capacity = capacity;
}

void patchChunk(Chunk* chunk, size_t position, uint8_t byte)
{
    chunk->data[position] = byte;
}

void writeChunk(Chunk* chunk, uint8_t byte)
{
    if (chunk->capacity <= chunk->count) {
        resizeChunk(chunk, chunk->capacity * 2);
    }

    chunk->data[chunk->count] = byte;
    chunk->count++;
}
