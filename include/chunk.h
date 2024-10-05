#ifndef CHUNK_H
#define CHUNK_H

#include "function.h"
#include "value.h"
#include <stddef.h>
#include <stdint.h>

#define CHUNK_INIT_CAPACITY 4

typedef struct Chunk
{
    uint8_t* data;
    size_t capacity;
    size_t count;
    FunctionArray functions;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
size_t countChunk(Chunk* chunk);
void resizeChunk(Chunk* chunk, size_t capacity);
void patchChunk(Chunk* chunk, size_t position, uint8_t byte);
void writeChunk(Chunk* chunk, uint8_t byte);

#endif
