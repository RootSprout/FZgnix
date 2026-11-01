#include "pool.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NG_POOL_DEFAULT_BLOCK 16384

struct ng_pool_block {
    ng_pool_block_t *next;
    size_t used;
    size_t capacity;
    max_align_t alignment;
    unsigned char data[];
};

typedef struct ng_pool_large {
    struct ng_pool_large *next;
    void *memory;
} ng_pool_large_t;

struct ng_pool {
    size_t block_size;
    ng_pool_block_t *blocks;
    ng_pool_block_t *current;
    ng_pool_large_t *large;
};

static ng_pool_block_t *block_create(size_t capacity) {
    if (capacity > SIZE_MAX - sizeof(ng_pool_block_t)) return NULL;
    ng_pool_block_t *block = malloc(sizeof(*block) + capacity);
    if (!block) return NULL;
    block->next = NULL;
    block->used = 0;
    block->capacity = capacity;
    return block;
}

ng_pool_t *ng_pool_create(size_t block_size) {
    if (block_size == 0) block_size = NG_POOL_DEFAULT_BLOCK;
    ng_pool_t *pool = calloc(1, sizeof(*pool));
    if (!pool) return NULL;
    pool->blocks = block_create(block_size);
    if (!pool->blocks) {
        free(pool);
        return NULL;
    }
    pool->current = pool->blocks;
    pool->block_size = block_size;
    return pool;
}

void *ng_pool_alloc(ng_pool_t *pool, size_t size) {
    if (!pool || size == 0) return NULL;
    const size_t alignment = _Alignof(max_align_t);

    if (size <= pool->block_size) {
        for (ng_pool_block_t *block = pool->current; block; block = block->next) {
            size_t aligned = (block->used + alignment - 1) & ~(alignment - 1);
            if (aligned <= block->capacity && size <= block->capacity - aligned) {
                void *memory = block->data + aligned;
                block->used = aligned + size;
                pool->current = block;
                return memory;
            }
        }
        ng_pool_block_t *block = block_create(pool->block_size);
        if (!block) return NULL;
        ng_pool_block_t *tail = pool->blocks;
        while (tail->next) tail = tail->next;
        tail->next = block;
        pool->current = block;
        block->used = size;
        return block->data;
    }

    void *memory = malloc(size);
    if (!memory) return NULL;
    ng_pool_large_t *large = malloc(sizeof(*large));
    if (!large) {
        free(memory);
        return NULL;
    }
    large->memory = memory;
    large->next = pool->large;
    pool->large = large;
    return memory;
}

void *ng_pool_calloc(ng_pool_t *pool, size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) return NULL;
    size_t total = count * size;
    void *memory = ng_pool_alloc(pool, total);
    if (memory) memset(memory, 0, total);
    return memory;
}

void ng_pool_destroy(ng_pool_t *pool) {
    if (!pool) return;
    ng_pool_large_t *large = pool->large;
    while (large) {
        ng_pool_large_t *next = large->next;
        free(large->memory);
        free(large);
        large = next;
    }
    ng_pool_block_t *block = pool->blocks;
    while (block) {
        ng_pool_block_t *next = block->next;
        free(block);
        block = next;
    }
    free(pool);
}
