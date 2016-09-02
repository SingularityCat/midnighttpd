#ifndef MIG_DYNARRAY_H
#define MIG_DYNARRAY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>

#define MIG_DYNARRAY_DEFAULT_CHUNK_MULTIPLIER 64

enum mig_dynarray_overflow_mode {
    MIG_DYNARRAY_GROW,
    MIG_DYNARRAY_NOOP
};

enum mig_dynarray_initopts {
    MIG_DYNARRAY_DEFAULT = 0,       /* Does nothing. */
    MIG_DYNARRAY_IGNORE_STRUCT = 1, /* Assume the existing data inside the struct is wrong. */
    MIG_DYNARRAY_FIXED_SIZE = 2,    /* Sets overflow behaviour to DYNARRAY_NOOP. */
    MIG_DYNARRAY_TIGHT_ALLOC = 4    /* Sets chunk multiplier to 1. */
};

struct mig_dynarray {
    size_t alignment;           /* Byte boundary to align items on. */
    size_t actual_size;         /* Actual size of an item. */
    size_t padded_size;         /* Padded size of an item. Equal to ceil(unit_size / alignment) * alignment. */
    size_t dynarray_size;       /* Size in bytes of the memory allocated. */
    size_t chunk_multiplier;    /* Dynarray will grow by this many items each time. */
    size_t minimum_size;        /* Dynarray will not go below this many bytes. */

    enum mig_dynarray_overflow_mode overflow_mode;

    ptrdiff_t sp;
    char *base;

    bool struct_alloced;
};

struct mig_dynarray *mig_dynarray_create(void);
void mig_dynarray_destroy(struct mig_dynarray *arr);

/* Private(ish) internally used memory managment functions */
bool mig_dynarray_grow(struct mig_dynarray *arr);
bool mig_dynarray_shrink(struct mig_dynarray *arr);

/* mig_dynarray property alteration functions. */
void mig_dynarray_init(struct mig_dynarray *arr, size_t usize, size_t alignment, unsigned int icount, enum mig_dynarray_initopts options);
size_t mig_dynarray_getchunkmul(struct mig_dynarray *arr);
void mig_dynarray_setchunkmul(struct mig_dynarray *arr, size_t mul);
enum mig_dynarray_overflow_mode dynarray_getmode(struct mig_dynarray *arr);
void mig_dynarray_setmode(struct mig_dynarray *arr, enum mig_dynarray_overflow_mode mode);

bool mig_dynarray_pushref(struct mig_dynarray *arr, void **ref);
bool mig_dynarray_popref(struct mig_dynarray *arr, void **ref);
bool mig_dynarray_peekref(struct mig_dynarray *arr, void **ref);

bool mig_dynarray_push(struct mig_dynarray *arr, const void *loc);
bool mig_dynarray_pop(struct mig_dynarray *arr, void *loc);
bool mig_dynarray_peek(struct mig_dynarray *arr, void *loc);

size_t mig_dynarray_len(struct mig_dynarray *arr);
bool mig_dynarray_indexref(struct mig_dynarray *arr, size_t idx, void **ref);

bool mig_dynarray_set(struct mig_dynarray *arr, size_t idx, const void *loc);
bool mig_dynarray_get(struct mig_dynarray *arr, size_t idx, void *loc);

bool mig_dynarray_to_array(struct mig_dynarray *arr, size_t *size, size_t *len, void **mem);

#endif
