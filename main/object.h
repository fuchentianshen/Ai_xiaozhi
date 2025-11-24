#pragma once

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "esp_heap_caps.h"

static inline void *object_create(size_t size)
{
    void *object = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!object)
    {
        return NULL;
    }
    memset(object, 0, size);
    return object;
}