// aric.c
// 03-Dec-15  Markku-Juhani O. Saarinen <mjos@iki.fi>
// Dynamic arithmetic coding / decoding routines.

#include "wee.h"

// Initialize frequencies to "balanced".

void aric_freqinit(uint32_t freq[][2], size_t bits)
{
    size_t i;

    for (i = 0; i < (1 << bits); i++) {
        freq[i][0] = 1;
        freq[i][1] = 1;
    }
}

// Divide frequencies in half, "de-emphasizing past".

void aric_freqhalf(uint32_t freq[][2], size_t bits)
{
    size_t i;

    for (i = 0; i < (1 << bits); i++) {
        if (freq[i][0] > 0)
            freq[i][0] >>= 1;
        if (freq[i][1] > 0)
            freq[i][1] >>= 1;
    }
}

// Update a frequency distribution for "bits"-sized word x.

void aric_addfreq(uint32_t freq[][2], size_t bits, uint32_t x)
{
    uint32_t i, b;

    for (i = 0; i < bits; i++) {
        b = 1 << i;
        if (x & b) {
            freq[x][1]++;               // bit is "1"
            x -= b;                     // clear it
        } else {
            freq[x + b][0]++;           // bit is "0"
        }
    }
}

// Initialize range buffer.

void aric_init_rb(aric_rb_t *rb, void *buf, size_t len, int dec)
{
    rb->buf = buf;                      // output buffer
    rb->max = len;                      // maximum pointer
    rb->ptr = 0;                        // byte pointer 0..max-1
    rb->bit = 0;                        // bit count 0..7
    rb->byt = 0;                        // (partial) output byte
    rb->b = 0x00000000;                 // lower bound
    rb->l = 0xFFFFFFFF;                 // range
    rb->v = 0;                          // input, initialized as 0

    // initialize input
    if (dec && len >= 5) {
        for (rb->ptr = 0; rb->ptr < 4; rb->ptr++) {
            rb->v <<= 8;
            rb->v += (uint32_t) rb->buf[rb->ptr];
        }
        rb->byt = rb->buf[rb->ptr++];
        rb->bit = 0;
    }
}

// Finalize output.

size_t aric_final_out(aric_rb_t *rb)
{
    int i;

    while (rb->bit < 8) {               // flush output byte
        rb->byt = (rb->byt << 1) ^ (rb->b >> 31);
        rb->b <<= 1;
        rb->bit++;
    }
    rb->buf[rb->ptr] = rb->byt & 0xFF;

    // carry out
    for (i = rb->ptr - 1; rb->byt >= 0x100 && i >= 0; i--) {
        rb->byt >>= 8;
        rb->byt += (uint32_t) rb->buf[i];
        rb->buf[i] = rb->byt & 0xFF;
    }
    rb->ptr++;

    for (i = 24; i >= 0 && rb->ptr < rb->max; i -= 8) {
        rb->buf[rb->ptr++] = (rb->b >> i) & 0xFF;
    }

    return rb->ptr;
}

// Encode a "bits"-sized word to output stream.

int aric_enc(aric_rb_t *rb,             // output stream
    uint32_t iwrd,                      // input word to be encoded
    uint32_t freq[][2], size_t bits)    // binary frequencies
{
    int i, ibit;
    uint32_t tree;                      // input word masked
    uint32_t f;                         // select midpoint

    for (ibit = bits - 1; ibit >= 0; ibit--) {

        // midpoint split
        if (freq == NULL) {
            f = rb->l >> 1;
        } else {
            tree = (iwrd & (~1 << ibit)) | (1 << ibit);
            f = ((uint64_t) rb->l) * ((uint64_t) freq[tree][0]) /
                ((uint64_t) freq[tree][0] + freq[tree][1]);
        }

        if (((iwrd >> ibit) & 1) == 0) {
            rb->l = f;                  // 0 bit; lower part
        } else {
            rb->b += f;                 // 1 bit; higher part
            rb->l -= f;                 // flip range to upper half
            if (rb->b < f)              // overflow ?
                rb->byt++;              // carry!
        }

        // normalize and output bits
        while (rb->l < 0x80000000) {
            rb->byt <<= 1;
            rb->byt += (rb->b >> 31) & 1;
            rb->bit++;
            if (rb->bit >= 8) {         // full byte ?
                rb->buf[rb->ptr] = rb->byt & 0xFF;

                // carry propagation
                for (i = rb->ptr - 1; rb->byt >= 0x100 && i >= 0; i--) {
                    rb->byt >>= 8;
                    rb->byt += (uint32_t) rb->buf[i];
                    rb->buf[i] = rb->byt & 0xFF;
                }
                rb->ptr++;
                if (rb->ptr >= rb->max) // output buffer overflow
                    return -1;
                rb->bit = 0;
                rb->byt = 0x00;
            }

            rb->b <<= 1;                // shift left
            rb->l <<= 1;                // double range
        }
    }

    return 0;
}

// Decode input word.

uint32_t aric_dec(aric_rb_t *rb,        // input range buffer
    uint32_t freq[][2], size_t bits)    // binary frequencies
{
    int obit;
    uint32_t f;
    uint32_t owrd, tree;

    owrd = 0;
    for (obit = bits - 1; obit >= 0; obit--) {

        // midpoint split
        if (freq == NULL) {
            f = rb->l >> 1;
        } else {
            tree = (owrd & (~1 << obit)) | (1 << obit);
            f = ((uint64_t) rb->l) * ((uint64_t) freq[tree][0]) /
                ((uint64_t) freq[tree][0] + freq[tree][1]);
        }

        if (rb->v - rb->b < f) {        // compare
            rb->l = f;                  // 0 bit; lower part
        } else {
            rb->b += f;                 // 1 bit; higher part
            rb->l -= f;                 // flip range to upper half
            owrd |= 1 << obit;          // set the bit
        }

        while (rb->l < 0x80000000) {
            rb->v <<= 1;                // fetch new bit
            rb->v += (rb->byt >> 7) & 1;
            rb->byt <<= 1;
            rb->bit++;
            if (rb->bit >= 8) {
                if (rb->ptr >= rb->max) // too much read!
                    return ~0;          // return error
                rb->bit = 0;
                rb->byt = rb->buf[rb->ptr++];
            }

            rb->b <<= 1;                // shift left
            rb->l <<= 1;                // double range
        }
    }

    return owrd;
}

