#include <stdlib.h>
#include <strings.h>
#include <stdalign.h>

#include "mig_dynarray.h"

struct mig_dynarray *mig_dynarray_create(void)
{
    struct mig_dynarray *arr = malloc(sizeof(struct mig_dynarray));
    if(arr == NULL)
    {
        return NULL;
    }

    /* Sane defaults */
    arr->dynarray_size = 0;
    arr->alignment = 1;
    arr->unit_size = 1;
    arr->chunk_multiplier = MIG_DYNARRAY_DEFAULT_CHUNK_MULTIPLIER;
    arr->minimum_size = 0;

    arr->overflow_mode = MIG_DYNARRAY_GROW;

    arr->sp = -1;
    arr->base = NULL;

    arr->struct_alloced = true;

    return arr;
}

void mig_dynarray_destroy(struct mig_dynarray *arr)
{
    free(arr->base);

    if(arr->struct_alloced)
    {
        free(arr);
    }
}

bool mig_dynarray_grow(struct mig_dynarray *arr)
{
    bool retval = true;
    size_t new_size = arr->dynarray_size + (arr->chunk_multiplier * arr->alignment);
    char *baseptr = realloc(arr->base, new_size);
    if(baseptr == NULL)
    {
        retval = false;
    }
    else
    {
        arr->dynarray_size = new_size;
        arr->base = baseptr;
    }

    return retval;
}

bool mig_dynarray_shrink(struct mig_dynarray *arr)
{
    bool retval = true;
    size_t new_size = arr->dynarray_size - (arr->chunk_multiplier * arr->alignment);
    char *baseptr;

    if(new_size < arr->minimum_size)
    {
        retval = false;
    }
    else
    {
        baseptr = realloc(arr->base, new_size);
        if(baseptr == NULL)
        {
            retval = false;
        }
        else
        {
            arr->dynarray_size = new_size;
            arr->base = baseptr;
            if(arr->sp > (arr->dynarray_size - arr->alignment))
            {
                /* Truncate dynarray pointer to highest valid index */
                arr->sp = arr->dynarray_size - arr->alignment;
            }
        }
    }

    return retval;
}

void mig_dynarray_init(struct mig_dynarray *arr, size_t usize, size_t alignment, unsigned int icount, enum mig_dynarray_initopts options)
{
    if((options & MIG_DYNARRAY_IGNORE_STRUCT) > 0)
    {
        arr->struct_alloced = false;
    }
    else
    {
        free(arr->base);
    }

    if((options & MIG_DYNARRAY_FIXED_SIZE) > 0)
    {
        arr->overflow_mode = MIG_DYNARRAY_NOOP;
    }
    else
    {
        arr->overflow_mode = MIG_DYNARRAY_GROW;
    }

    if((options & MIG_DYNARRAY_TIGHT_ALLOC) > 0)
    {
        arr->chunk_multiplier = 1;
    }
    else
    {
        arr->chunk_multiplier = MIG_DYNARRAY_DEFAULT_CHUNK_MULTIPLIER;
    }

    div_t alnqr = div(alignment, alignof(void *));

    if(alnqr.quot == 0)
    {
        alignment--;
        for(int i = 0; i <= ffs(sizeof(void *)); i++)
        {
            alignment |= alignment >> (1 << (i - 1));
        }
        alignment++;
    }
    else
    {
        alignment = (alnqr.quot + (alnqr.rem > 0 ? 1 : 0)) * alignof(void *);
    }

    if(alignment < usize)
    {
        alignment += (alignment * (usize / alignment));
    }

    arr->minimum_size = icount * alignment;
    arr->alignment = alignment;
    arr->unit_size = usize;
    arr->sp = -((ptrdiff_t) alignment);
    arr->base = aligned_alloc(alignment, arr->minimum_size);
    arr->dynarray_size = arr->minimum_size;
}

/* mig_dynarray property alteration functions. */
size_t mig_dynarray_getchunkmul(struct mig_dynarray *arr)
{
    return arr->chunk_multiplier;
}

void mig_dynarray_setchunkmul(struct mig_dynarray *arr, size_t mul)
{
    arr->chunk_multiplier = mul;
}

enum mig_dynarray_overflow_mode dynarray_getmode(struct mig_dynarray *arr)
{
    return arr->overflow_mode;
}

void mig_dynarray_setmode(struct mig_dynarray *arr, enum mig_dynarray_overflow_mode mode)
{
    arr->overflow_mode = mode;
}

/* Standard dynarray operations that manipulate the stack pointer and return pointers to parts of the stack. */

bool mig_dynarray_pushref(struct mig_dynarray *arr, void **ref)
{
    bool retval = false; /* Set this to false initially. */
    arr->sp += arr->alignment;
    if(arr->sp > 0 && arr->sp >= arr->dynarray_size)
    {
        //logMessage(LOG_DEBUG, "Overflow in dynarray (%p).", (void *) arr);
        switch(arr->overflow_mode)
        {
            case MIG_DYNARRAY_GROW:
                retval = mig_dynarray_grow(arr); /* Returns true on success. */
                break;
            case MIG_DYNARRAY_NOOP:
                break;
        }
    }
    else
    {
        retval = true;
    }

    /* If retval is true at this point, memory is availible.*/
    if(retval)
    {
        *ref = (void *)(arr->base + arr->sp);
    }
    else
    {
        *ref = NULL;
    }

    return retval;
}

bool mig_dynarray_popref(struct mig_dynarray *arr, void **ref)
{
    bool retval = true;
    if(arr->sp < 0)
    {
        //logMessage(LOG_DEBUG, "Underflow in dynarray (%p).", (void *) arr);
        retval = false;
        *ref = NULL;
    }
    else
    {
        *ref = (void *)(arr->base + arr->sp);
        arr->sp -= arr->alignment;
    }

    return retval;
}

bool mig_dynarray_peekref(struct mig_dynarray *arr, void **ref)
{
    bool retval = true;
    if(arr->sp < 0)
    {
        retval = false;
        *ref = NULL;
    }
    else
    {
        *ref = (void *)(arr->base + arr->sp);
    }

    return retval;
}

/* Standard dynarray operations that copy from/to a specified location, implemented in terms of their *Ref counterparts. */
bool mig_dynarray_push(struct mig_dynarray *arr, const void *loc)
{
    void *ptr;
    bool retval = mig_dynarray_pushref(arr, &ptr);
    
    /* If retval is true at this point, memory is availible.*/
    if(retval)
    {
        memcpy(ptr, loc, arr->unit_size);
    }

    return retval;
}

bool mig_dynarray_pop(struct mig_dynarray *arr, void *loc)
{
    void *ptr;
    bool retval = mig_dynarray_popref(arr, &ptr);
    
    if(retval)
    {
        memcpy(loc, ptr, arr->unit_size);
    }

    return retval;
}

bool mig_dynarray_peek(struct mig_dynarray *arr, void *loc)
{
    void *ptr;
    bool retval = mig_dynarray_peekref(arr, &ptr);;
    if(retval)
    {
        memcpy(loc, ptr, arr->unit_size);
    }
    return retval;
}

size_t mig_dynarray_len(struct mig_dynarray *arr)
{
    return (arr->sp / (ptrdiff_t) arr->alignment) + 1;
}

bool mig_dynarray_indexref(struct mig_dynarray *arr, size_t idx, void **ref)
{
    bool retval = true;
    size_t offset = idx * arr->alignment;
    if(offset > arr->sp)
    {
        retval = false;
        *ref = NULL;
    }
    else
    {
        *ref = (void *)(arr->base + offset);
    }
    
    return retval;
}

bool mig_dynarray_set(struct mig_dynarray *arr, size_t idx, const void *loc)
{
    void *ptr;
    bool retval = mig_dynarray_indexref(arr, idx, &ptr);

    if(retval)
    {
        memcpy(ptr, loc, arr->unit_size);
    }

    return retval;
}

bool mig_dynarray_get(struct mig_dynarray *arr, size_t idx, void *loc)
{
    void *ptr;
    bool retval = mig_dynarray_indexref(arr, idx, &ptr);

    if(retval)
    {
        memcpy(loc, ptr, arr->unit_size);
    }

    return retval;
}

bool mig_dynarray_to_array(struct mig_dynarray *arr, size_t *size, size_t *len, void **mem)
{
    size_t arrsize;
    size_t arrlen;
    bool sizereturned;
    bool retval = true;
    void *array;

    arrsize = arr->sp + arr->alignment;
    arrlen = arrsize / arr->alignment;

    if(mem == NULL)
    {
        //logMessage(LOG_ERROR, "dynarrayToArray: mem is NULL. Not allocating to avoid memory leak.");
        retval = false;
    }
    else
    {

        array = malloc(arrsize);
        memcpy(array, arr->base, arrsize);
        *mem = array;

        if(!((size != NULL ? *size = arrsize, true : false) | (len  != NULL ? *len  = arrlen,  true : false)))
        {
            //logMessage(LOG_WARNING, "dynarrayToArray: Caller cannot possibly know size/length of array %p without external tracking.", array);
        }
    }

    return retval;
}
