#include "chunk.h"
#include <stdlib.h>
#include <stdio.h>

void initChunk(Chunk* chunk)
{
    chunk->capacity = CHUNK_INIT_CAPACITY;
    chunk->code = calloc(chunk->capacity, sizeof(uint8_t));
    chunk->count = 0;

    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk)
{
    free(chunk->code);
}

size_t countChunk(Chunk* chunk)
{
    return chunk->count;
}

void resizeChunk(Chunk* chunk, size_t capacity)
{
    chunk->code = realloc(chunk->code, sizeof(uint8_t) * capacity);
    chunk->capacity = capacity;
}

void patchChunk(Chunk* chunk, size_t position, uint8_t byte)
{
    chunk->code[position] = byte;
}

void writeChunk(Chunk* chunk, uint8_t byte)
{
    if (chunk->capacity <= chunk->count) {
        resizeChunk(chunk, chunk->capacity * 2);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}
