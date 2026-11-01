#ifndef NGNIX_CORE_POOL_H
#define NGNIX_CORE_POOL_H

#include <stddef.h>

typedef struct ng_pool_block ng_pool_block_t;
typedef struct ng_pool ng_pool_t;

ng_pool_t *ng_pool_create(size_t block_size);
void *ng_pool_alloc(ng_pool_t *pool, size_t size);
void *ng_pool_calloc(ng_pool_t *pool, size_t count, size_t size);
void ng_pool_destroy(ng_pool_t *pool);

#endif
