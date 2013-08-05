/** The hash function used here is by Bob Jenkins, 1996:
 *  <http://burtleburtle.net/bob/hash/doobs.html>
 *     "By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.
 *     You may use this code any way you wish, private, educational,
 *     or commercial.  It's free." */
#include "hash.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
uint32_t hash(const void *key, size_t length, const uint32_t initval) {
    uint32_t a,b,c; /* internal state */
    union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;
    u.ptr = key;

    if ((u.i & 0x3) == 0) {
        const uint32_t *k = (uint32_t*) key;

        /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
        for (; length > 12; length -= 12, k += 3) {
            a += k[0];
            b += k[1];
            c += k[2];
            mix(a,b,c);
        }

        /*----------------------------- handle the last (probably partial) block */
        /*
         * "k[2]&0xffffff" actually reads beyond the end of the string, but
         * then masks off the part it's not allowed to read.  Because the
         * string is aligned, the masked-off tail is in the same word as the
         * rest of the string.  Every machine with memory protection I've seen
         * does it on word boundaries, so is OK with this.  But VALGRIND will
         * still catch it and complain.  The masking trick does make the hash
         * noticably faster for short strings (like English words).
         */
        switch(length) {
            case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
            case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
            case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
            case 9: c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
            case 8: b+=k[1]; a+=k[0]; break;
            case 7: b+=k[1]&0xffffff; a+=k[0]; break;
            case 6: b+=k[1]&0xffff; a+=k[0]; break;
            case 5: b+=k[1]&0xff; a+=k[0]; break;
            case 4: a+=k[0]; break;
            case 3: a+=k[0]&0xffffff; break;
            case 2: a+=k[0]&0xffff; break;
            case 1: a+=k[0]&0xff; break;
            case 0: return c;  /* zero length strings require no mixing */
        }
    }
    else if ((u.i & 0x1) == 0) {
        const uint16_t *k = (uint16_t*) key; /* read 16-bit chunks */

        /*--------------- all but last block: aligned reads and different mixing */
        for (; length > 12; length -= 12, k += 6) {
            a += k[0] + (((uint32_t)k[1])<<16);
            b += k[2] + (((uint32_t)k[3])<<16);
            c += k[4] + (((uint32_t)k[5])<<16);
            mix(a,b,c);
        }

        /*----------------------------- handle the last (probably partial) block */
        const uint8_t *k8 = (const uint8_t *)k;
        switch(length) {
            case 12:
                c+=k[4]+(((uint32_t)k[5])<<16);
                b+=k[2]+(((uint32_t)k[3])<<16);
                a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 11: c+=((uint32_t)k8[10])<<16; /* @fallthrough */
            case 10:
                c+=k[4]; /* @fallthrough@ */
                b+=k[2]+(((uint32_t)k[3])<<16);
                a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 9: c+=k8[8]; /* @fallthrough */
            case 8:
                b+=k[2]+(((uint32_t)k[3])<<16);
                a+=k[0]+(((uint32_t)k[1])<<16);
                break;
            case 7: b+=((uint32_t)k8[6])<<16;      /* @fallthrough */
            case 6: b+=k[2]; a+=k[0]+(((uint32_t)k[1])<<16); break;
            case 5: b+=k8[4];  /* @fallthrough */
            case 4: a+=k[0]+(((uint32_t)k[1])<<16); break;
            case 3: a+=((uint32_t)k8[2])<<16;  /* @fallthrough */
            case 2: a+=k[0]; break;
            case 1: a+=k8[0]; break;
            case 0 : return c;  /* zero length strings require no mixing */
        }
    } else {                        /* need to read the key one byte at a time */
        const uint8_t *k = (uint8_t*) key;

        /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
        for (; length > 12; length -= 12, k += 12) {
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
        }

        /*-------------------------------- last block: affect all 32 bits of (c) */
        switch(length) {                  /* all the case statements fall through */
            case 12: c+=((uint32_t)k[11])<<24;
            case 11: c+=((uint32_t)k[10])<<16;
            case 10: c+=((uint32_t)k[9])<<8;
            case 9: c+=k[8];
            case 8: b+=((uint32_t)k[7])<<24;
            case 7: b+=((uint32_t)k[6])<<16;
            case 6: b+=((uint32_t)k[5])<<8;
            case 5: b+=k[4];
            case 4: a+=((uint32_t)k[3])<<24;
            case 3: a+=((uint32_t)k[2])<<16;
            case 2: a+=((uint32_t)k[1])<<8;
            case 1: a+=k[0]; break;
            case 0: return c;  /* zero length strings require no mixing */
        }
    }

    finalmix(a,b,c);

    return c; /* zero length strings require no mixing */
}
#elif __BYTE_ORDER == __BIG_ENDIAN
uint32_t hash( const void *key, size_t length, const uint32_t initval) {
    uint32_t a,b,c;
    union { const void *ptr; size_t i; } u; /* to cast key to (size_t) happily */

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;
    u.ptr = key;

    if ((u.i & 0x3) == 0) {
        const uint32_t *k = (uint32_t*) key; // read 32-bit chunks

        /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
        for (; length > 12; length -= 12, k += 3) {
            a += k[0];
            b += k[1];
            c += k[2];
            mix(a,b,c);
        }

        /*----------------------------- handle the last (probably partial) block */
        /*
         * "k[2]<<8" actually reads beyond the end of the string, but
         * then shifts out the part it's not allowed to read.  Because the
         * string is aligned, the illegal read is in the same word as the
         * rest of the string.  Every machine with memory protection I've seen
         * does it on word boundaries, so is OK with this.  But VALGRIND will
         * still catch it and complain.  The masking trick does make the hash
         * noticably faster for short strings (like English words).
         */
        switch(length) {
            case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
            case 11: c+=k[2]&0xffffff00; b+=k[1]; a+=k[0]; break;
            case 10: c+=k[2]&0xffff0000; b+=k[1]; a+=k[0]; break;
            case 9: c+=k[2]&0xff000000; b+=k[1]; a+=k[0]; break;
            case 8: b+=k[1]; a+=k[0]; break;
            case 7: b+=k[1]&0xffffff00; a+=k[0]; break;
            case 6: b+=k[1]&0xffff0000; a+=k[0]; break;
            case 5: b+=k[1]&0xff000000; a+=k[0]; break;
            case 4: a+=k[0]; break;
            case 3: a+=k[0]&0xffffff00; break;
            case 2: a+=k[0]&0xffff0000; break;
            case 1: a+=k[0]&0xff000000; break;
            case 0: return c; /* zero length strings require no mixing */
        }
    }
    else { /* need to read the key one byte at a time */
        const uint8_t *k = (uint8_t*) key;

        /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
        for (; length > 12; length -= 12, k += 12) {
            a += ((uint32_t)k[0])<<24;
            a += ((uint32_t)k[1])<<16;
            a += ((uint32_t)k[2])<<8;
            a += ((uint32_t)k[3]);
            b += ((uint32_t)k[4])<<24;
            b += ((uint32_t)k[5])<<16;
            b += ((uint32_t)k[6])<<8;
            b += ((uint32_t)k[7]);
            c += ((uint32_t)k[8])<<24;
            c += ((uint32_t)k[9])<<16;
            c += ((uint32_t)k[10])<<8;
            c += ((uint32_t)k[11]);
            mix(a,b,c);
            length -= 12;
            k += 12;
        }
        /*-------------------------------- last block: affect all 32 bits of (c) */
        switch(length) {  /* all the case statements fall through */
            case 12: c+=k[11];
            case 11: c+=((uint32_t)k[10])<<8;
            case 10: c+=((uint32_t)k[9])<<16;
            case 9: c+=((uint32_t)k[8])<<24;
            case 8: b+=k[7];
            case 7: b+=((uint32_t)k[6])<<8;
            case 6: b+=((uint32_t)k[5])<<16;
            case 5: b+=((uint32_t)k[4])<<24;
            case 4: a+=k[3];
            case 3: a+=((uint32_t)k[2])<<8;
            case 2: a+=((uint32_t)k[1])<<16;
            case 1: a+=((uint32_t)k[0])<<24; break;
            case 0: return c;
        }
    }

    finalmix(a,b,c);

    return c;
}
#else
#error "some kind of #smerdo"
#endif
