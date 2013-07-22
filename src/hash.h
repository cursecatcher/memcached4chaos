#ifndef __HASH_H__
#define __HASH_H__

#include <inttypes.h> //declaration of uint*_t
#include <stddef.h> //declaration of size_t

#ifdef __cplusplus
extern "C" {
#endif

uint32_t hash(const void *key, size_t length, const uint32_t initval = 0);

#ifdef __cplusplus
}
#endif

#endif
