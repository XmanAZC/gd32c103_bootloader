#include <stddef.h>

void* memset(void *s, int c, size_t n)
{
#define LBLOCKSIZE (sizeof(unsigned long))
#define UNALIGNED(X) ((long)X & (LBLOCKSIZE - 1))
#define TOO_SMALL(LEN) ((LEN) < LBLOCKSIZE)

    unsigned int i = 0;
    char *m = (char *)s;
    unsigned long buffer = 0;
    unsigned long *aligned_addr = NULL;
    unsigned char d = (unsigned int)c & (unsigned char)(-1); /* To avoid sign extension, copy C to an
                               unsigned variable. (unsigned)((char)(-1))=0xFF for 8bit and =0xFFFF for 16bit: word independent */

    if (!TOO_SMALL(n) && !UNALIGNED(s))
    {
        /* If we get this far, we know that n is large and s is word-aligned. */
        aligned_addr = (unsigned long *)s;

        /* Store d into each char sized location in buffer so that
         * we can set large blocks quickly.
         */
        for (i = 0; i < LBLOCKSIZE; i++)
        {
            *(((unsigned char *)&buffer) + i) = d;
        }

        while (n >= LBLOCKSIZE * 4)
        {
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            n -= 4 * LBLOCKSIZE;
        }

        while (n >= LBLOCKSIZE)
        {
            *aligned_addr++ = buffer;
            n -= LBLOCKSIZE;
        }

        /* Pick up the remainder with a bytewise loop. */
        m = (char *)aligned_addr;
    }

    while (n--)
    {
        *m++ = (char)d;
    }

    return s;

#undef LBLOCKSIZE
#undef UNALIGNED
#undef TOO_SMALL
}

void* memcpy(void *dst, const void *src, size_t count)
{
#define UNALIGNED(X, Y) \
    (((long)X & (sizeof(long) - 1)) | ((long)Y & (sizeof(long) - 1)))
#define BIGBLOCKSIZE (sizeof(long) << 2)
#define LITTLEBLOCKSIZE (sizeof(long))
#define TOO_SMALL(LEN) ((LEN) < BIGBLOCKSIZE)

    char *dst_ptr = (char *)dst;
    char *src_ptr = (char *)src;
    long *aligned_dst = NULL;
    long *aligned_src = NULL;
    size_t len = count;

    /* If the size is small, or either SRC or DST is unaligned,
    then punt into the byte copy loop.  This should be rare. */
    if (!TOO_SMALL(len) && !UNALIGNED(src_ptr, dst_ptr))
    {
        aligned_dst = (long *)dst_ptr;
        aligned_src = (long *)src_ptr;

        /* Copy 4X long words at a time if possible. */
        while (len >= BIGBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            len -= BIGBLOCKSIZE;
        }

        /* Copy one long word at a time if possible. */
        while (len >= LITTLEBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            len -= LITTLEBLOCKSIZE;
        }

        /* Pick up any residual with a byte copier. */
        dst_ptr = (char *)aligned_dst;
        src_ptr = (char *)aligned_src;
    }

    while (len--)
        *dst_ptr++ = *src_ptr++;

    return dst;
#undef UNALIGNED
#undef BIGBLOCKSIZE
#undef LITTLEBLOCKSIZE
#undef TOO_SMALL
}
