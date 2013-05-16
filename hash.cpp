#include "hash.hpp"


uint32_t hash::hash_function(const void* key, size_t length, const uint32_t initval) {
    uint32_t a,b,c;
    union { const void *ptr; size_t i; } u;

    a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

    u.ptr = key;
    if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
        const uint32_t *k = (uint32_t *)key;

#ifdef VALGRIND
        const uint8_t  *k8;
#endif
        while (length > 12) {
            a += k[0];
            b += k[1];
            c += k[2];
            mix(a,b,c);
            length -= 12;
            k += 3;
        }

#ifndef VALGRIND

        switch(length) {
            case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
            case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
            case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
            case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
            case 8 : b+=k[1]; a+=k[0]; break;
            case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
            case 6 : b+=k[1]&0xffff; a+=k[0]; break;
            case 5 : b+=k[1]&0xff; a+=k[0]; break;
            case 4 : a+=k[0]; break;
            case 3 : a+=k[0]&0xffffff; break;
            case 2 : a+=k[0]&0xffff; break;
            case 1 : a+=k[0]&0xff; break;
            case 0 : return c;
        }

#else
        k8 = (const uint8_t *)k;
        switch(length) {
            case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
            case 11: c+=((uint32_t)k8[10])<<16;  /* fall through */
            case 10: c+=((uint32_t)k8[9])<<8;    /* fall through */
            case 9 : c+=k8[8];                   /* fall through */
            case 8 : b+=k[1]; a+=k[0]; break;
            case 7 : b+=((uint32_t)k8[6])<<16;   /* fall through */
            case 6 : b+=((uint32_t)k8[5])<<8;    /* fall through */
            case 5 : b+=k8[4];                   /* fall through */
            case 4 : a+=k[0]; break;
            case 3 : a+=((uint32_t)k8[2])<<16;   /* fall through */
            case 2 : a+=((uint32_t)k8[1])<<8;    /* fall through */
            case 1 : a+=k8[0]; break;
            case 0 : return c;
        }

    #endif

    }
    else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
        const uint16_t *k = (uint16_t *) key;                           /* read 16-bit chunks */
        const uint8_t  *k8;

        while (length > 12) {
            a += k[0] + (((uint32_t)k[1])<<16);
            b += k[2] + (((uint32_t)k[3])<<16);
            c += k[4] + (((uint32_t)k[5])<<16);
            mix(a,b,c);
            length -= 12;
            k += 6;
        }

        k8 = (const uint8_t *)k;
        switch(length) {
            case 12: c+=k[4]+(((uint32_t)k[5])<<16);
                b+=k[2]+(((uint32_t)k[3])<<16);
                a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 11: c+=((uint32_t)k8[10])<<16;     /* @fallthrough */
            case 10: c+=k[4];                       /* @fallthrough@ */
                b+=k[2]+(((uint32_t)k[3])<<16);
                a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 9 : c+=k8[8];                      /* @fallthrough */
            case 8 : b+=k[2]+(((uint32_t)k[3])<<16);
                a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 7 : b+=((uint32_t)k8[6])<<16;      /* @fallthrough */
            case 6 : b+=k[2];
                a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 5 : b+=k8[4];                      /* @fallthrough */
            case 4 : a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 3 : a+=((uint32_t)k8[2])<<16;      /* @fallthrough */
            case 2 : a+=k[0];
                break;
            case 1 : a+=k8[0];
                break;
            case 0 : return c;
        }
    }
    else {
        const uint8_t *k = (uint8_t *) key;

        while (length > 12) {
            a += k[0];
            a += ((uint32_t)k[1])<<8;
            a += ((uint32_t)k[2])<<16;
            a += ((uint32_t)k[3])<<24;
            b += k[4];
            b += ((uint32_t)k[5])<<8;
            b += ((uint32_t)k[6])<<16;
            b += ((uint32_t)k[7])<<24;
            c += k[8];
            c += ((uint32_t)k[9])<<8;
            c += ((uint32_t)k[10])<<16;
            c += ((uint32_t)k[11])<<24;
            mix(a,b,c);
            length -= 12;
            k += 12;
        }

        switch(length) {
            case 12: c+=((uint32_t)k[11])<<24;
            case 11: c+=((uint32_t)k[10])<<16;
            case 10: c+=((uint32_t)k[9])<<8;
            case 9 : c+=k[8];
            case 8 : b+=((uint32_t)k[7])<<24;
            case 7 : b+=((uint32_t)k[6])<<16;
            case 6 : b+=((uint32_t)k[5])<<8;
            case 5 : b+=k[4];
            case 4 : a+=((uint32_t)k[3])<<24;
            case 3 : a+=((uint32_t)k[2])<<16;
            case 2 : a+=((uint32_t)k[1])<<8;
            case 1 : a+=k[0]; break;
            case 0 : return c;
        }
    }

    final_mixing(a, b, c);

    return c;
}
