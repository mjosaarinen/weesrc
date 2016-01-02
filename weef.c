// weef.c
// 30-Dec-15  Markku-Juhani O. Saarinen <mjos@iki.fi>
// Encoding via block sorting dictionaries.

#include <stdlib.h>
#include <string.h>

#include "wee.h"

#ifndef WEE_BLK
#define WEE_BLK 0x100000
#endif
#ifndef WEE_BUF
#define WEE_BUF 0x1000
#endif
#ifndef WEE_SRT
#define WEE_SRT 0x100
#endif

#define WEE_MINDICT 5
#define WEE_OFHIST 5

// Comparator for an array of pointers

static int wee_compar(const void *a, const void *b)
{
    int d;
    d = memcmp(*((uint8_t **) a), *((uint8_t **) b), WEE_SRT);
    if (d == 0)                         // never return 0; sort together
        return a < b;
    return d;
}

// Return number of byte positions where two strings are equal

static int wee_equ(const uint8_t *a, const uint8_t *b, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (a[i] != b[i])
            return i;
    }

    return n;
}

// simple log2

static int wee_log2(uint32_t x)
{
    int l;

    l = 0;
    while (x > 0) {
        x >>= 1;
        l++;
    }

    return l;
}

void dbg_64(uint8_t *pt)
{
    int i, ch;

    printf("%02X ", pt[0]);

    for (i = 0; i < 64; i++) {
        ch = pt[i];
        putchar(ch >= 32 && ch < 127 ? ch : '.');
    }
    printf("\n");
}

// Encode a length.

void wee_enc_len(aric_rb_t *rbo, int l, uint32_t fr6[0x40][2])
{
    uint32_t x;

    if (l < 0) {                        // 33..36 are for special codes
        x = 32 - l;
        aric_enc(rbo, x, fr6, 6);
        aric_addfreq(fr6, 6, x);
        return;
    }

    if (l <= 32) {                      // short ?
        aric_enc(rbo, l, fr6, 6);
        aric_addfreq(fr6, 6, l);
    } else {
        x = wee_log2(l);                // encode bit length
        aric_enc(rbo, x + 32, fr6, 6);
        aric_addfreq(fr6, 6, x + 32);
        aric_enc(rbo, l, NULL, x - 1);  // actual bits
    }
}

// Decode a length.

int wee_dec_len(aric_rb_t *rbi, uint32_t fr6[0x40][2])
{
    uint32_t x, l;

    x = aric_dec(rbi, fr6, 6);          // decode
    aric_addfreq(fr6, 6, x);

    if (x <= 32)                        // plain value
        return x;

    x -= 32;
    if (x < 5)                          // special codes
        return -x;
    l = aric_dec(rbi, NULL, x - 1);     // get bits
    l |= 1 << (x - 1);                  // leading 1

    return l;
}

// Compress "fin" to "fout".

size_t wee_file_enc(FILE *fin, FILE *fout, int verb)
{
    // input buffer
    uint8_t     *din;                   // input buffer
    uint32_t    dip, dil;               // input pointer, len
    uint8_t     dou[WEE_BUF + 2 * 64];  // note: 64B surety at both ends
    aric_rb_t   rbo;                    // range buffer (out)
    uint32_t    f8x8[0x100][0x100][2];  // for literals
    uint32_t    fr6l[0x40][2];          // literal run lengths
    uint32_t    fr6o[0x40][2];          // repeat string offsets
    uint32_t    fr6s[0x40][2];          // repeat string lengths
    size_t      isz, osz;               // input and output size

    uint8_t     **srt;                  // sorted pointers
    uint32_t    *idx;                   // reverse index

    size_t      sle;                    // sorted len
    size_t      i;
    uint32_t    x, y, z, j;             // work variables
    uint32_t    ble, bof, lit;          // match len, offset, literal run
    uint32_t    pof[WEE_OFHIST];        // previous offsets
    int         l, a, b;                // len, current and previous bytes

    if ((din = calloc(3 * WEE_BLK, sizeof(uint8_t))) == NULL ||
        (srt = calloc(2 * WEE_BLK, sizeof(uint8_t *))) == NULL ||
        (idx = calloc(2 * WEE_BLK, sizeof(uint32_t))) == NULL) {
        perror("calloc()");
        exit(1);                        // no point continuing
    }

    fputc(0x07, fout);                  // magic "2016"
    fputc(0xE0, fout);

    for (i = 0; i < 0x100; i++)         // init frequencies
        aric_freqinit(f8x8[i], 8);
    aric_freqinit(fr6l, 6);
    aric_freqinit(fr6o, 6);
    aric_freqinit(fr6s, 6);

    for (i = 0; i < WEE_OFHIST; i++)    // previous offsets
        pof[i] = 0;

    dip = 0;                            // input pointer
    dil = 0;                            // input length
    isz = 0;                            // input size
    osz = 2;                            // bytes written (magic)
    lit = 0;                            // literal run

    a = 0x00;                           // current and previous bytes
    b = 0x00;

    memset(dou, 0x00, sizeof(dou));     // output buffer
    aric_init_rb(&rbo, dou, sizeof(dou), 0);

    while (dip <= dil) {

        // read as much as possible
        i = fread(&din[dil], 1, (3 * WEE_BLK) - dil, fin);
        isz += i;
        dil += i;

        if (dip >= dil)
            break;

        // clear rest
        memset(&din[dil], 0x00, (3 * WEE_BLK) - dil);

        sle = 2 * WEE_BLK;              // sort
        if (dil < sle)
            sle = dil;
        for (i = 0; i < sle; i++)
            srt[i] = &din[i];
        qsort(srt, sle, sizeof(uint8_t *), wee_compar);

        for (i = 0; i < sle; i++) {     // index
            idx[srt[i] - din] = i;
        }

        while (dip < dil && dip < 2 * WEE_BLK) {

            x = idx[dip];               // find the best match
            ble = 0;
            bof = 0;

            // scan up
            for (j = 1; j < 256 && j <= x; j++) {
                y = srt[x - j] - din;
                z = wee_equ(&din[dip], &din[y], dil - dip);
                if (z < WEE_MINDICT)
                    break;
                if (y < dip) {
                    ble = z;
                    bof = dip - y;
                    break;
                }
            }

            // scan down
            for (j = 1; j < 256 && x + j < sle; j++) {
                y = srt[x + j] - din;
                z = wee_equ(&din[dip], &din[y], dil - dip);
                if (z < WEE_MINDICT || z < ble)
                    break;
                if (y < dip) {
                    if (z > ble) {
                        ble = z;
                        bof = dip - y;
                    }
                    break;
                }
            }

            if (ble < WEE_MINDICT) {    // just proceed

                dip++;
                lit++;

            } else {                    // repeat string found

                // encode literals
                wee_enc_len(&rbo, lit, fr6l);
                for (i = 0; i < lit; i++) {
                    a = din[dip - lit + i];
                    aric_enc(&rbo, a, f8x8[b], 8);
                    aric_addfreq(f8x8[b], 8, a);
                    b = a;
                }
                lit = 0;

                // encode length
                wee_enc_len(&rbo, ble, fr6s);

                if (ble > 0) {          // encode offset
                    if (bof <= 32) {
                        wee_enc_len(&rbo, bof, fr6o);
                    } else {            // large offsets use a history feature
                        for (l = 0; l < WEE_OFHIST; l++) {
                            if (pof[l] == bof) {
                                wee_enc_len(&rbo, -l, fr6o);
                                break;
                            }
                        }
                        if (l == WEE_OFHIST) {
                            // not found in previous offsets
                            wee_enc_len(&rbo, bof, fr6o);
                            for (l = WEE_OFHIST - 1; l > 0; l--)
                                pof[l] = pof[l - 1];
                            pof[0] = bof;
                        }
                    }
                    dip += ble;         // advance pointer
                }
            }

            // output range buffer overflow ?
            if (rbo.ptr > rbo.max - 64) {
                i = rbo.ptr - 64;       // leave something for carry
                if (fwrite(dou, 1, i, fout) != i) {
                    perror("error writing");
                    return 0;
                }
                osz += i;
                rbo.ptr -= i;
                memmove(&dou, &dou[i], rbo.ptr);
            }
        }

        if (dip > WEE_BLK) {            // move data back
            i = dip - WEE_BLK;
            dip -= i;
            dil -= i;
            memmove(din, &din[i], dil);
        }
    }

    wee_enc_len(&rbo, lit, fr6l);       // encode remaining literals
    for (i = 0; i < lit; i++) {
        a = din[dip - lit + i];
        aric_enc(&rbo, a, f8x8[b], 8);
        aric_addfreq(f8x8[b], 8, a);
        b = a;
    }
    lit = 0;
    wee_enc_len(&rbo, -1, fr6s);        // unique end symbol; runlen = -1
    aric_final_out(&rbo);               // flush out buffer

    if (fwrite(dou, 1, rbo.ptr, fout) != rbo.ptr) {
        perror("error writing");
        return 0;
    }
    osz += rbo.ptr;

    if (verb) {                         // verbose statistics
        printf("%12zu %12zu  %.1f%%  ",
            isz, osz, 100.0 * ((double) isz - osz) / ((double) isz));
    }

    free(din);
    free(srt);
    free(idx);

    return osz;
}

// Decompress "fin" to "fout".

size_t wee_file_dec(FILE *fin, FILE *fout, int verb)
{
    uint8_t     din[WEE_BUF + 64];      // in buffer
    aric_rb_t   rbi;                    // range buffer (in)
    uint8_t     *dou;                   // out buffer
    uint32_t    dop;                    // output pointer
    uint32_t    f8x8[0x100][0x100][2];  // for literals
    uint32_t    fr6l[0x40][2];          // literal run lengths
    uint32_t    fr6o[0x40][2];          // for offsets
    uint32_t    fr6s[0x40][2];          // for string lengths
    size_t      i, isz, osz;            // looper, input size, output size
    uint32_t    rof, rle, lit;          // string offset, length
    uint32_t    pof[5];                 // previous offsets
    int         l, a, b;                // current and previous byte

    if (fgetc(fin) != 0x07 ||           // magic "2016"
        fgetc(fin) != 0xE0) {
        fprintf(stderr, "Invalid magic.\n");
        return 0;
    }

    if ((dou = calloc(3 * WEE_BLK, 1)) == NULL) {
        perror("calloc()");
        exit(1);
    }


    for (i = 0; i < 0x100; i++) {       // init frequencies
//      f8x8[i] = calloc(1, sizeof(freq8_t));
        aric_freqinit(f8x8[i], 8);
    }
    aric_freqinit(fr6l, 6);
    aric_freqinit(fr6o, 6);
    aric_freqinit(fr6s, 6);


    dop = 0;                            // output pointer
    isz = 2;                            // input size (magic)
    osz = 0;                            // number of bytes written
    for (i = 0; i < 5; i++)             // previous offsets
        pof[i] = 0;

    // read initial chunk
    memset(din, 0x00, sizeof(din));
    i = fread(din, 1, sizeof(din), fin);
    aric_init_rb(&rbi, din, i, 1);      // this gets the "v" param initialized
    isz += i;

    lit = wee_dec_len(&rbi, fr6l);      // first literal length

    a = 0x00;                           // current and previous bytes
    b = 0x00;

    while (rbi.ptr < rbi.max) {

        if (lit > 0) {                  // copy literals

            a = aric_dec(&rbi, f8x8[b], 8);
            aric_addfreq(f8x8[b], 8, a);
            dou[dop++] = a;
            b = a;
            lit--;

        } else {                        // repeat

            l = wee_dec_len(&rbi, fr6s);
            if (l == -1)                // end symbol
                break;
            rle = l;

            if (rle > 0) {              // repeat string offset
                l = wee_dec_len(&rbi, fr6o);
                if (l <= 0) {           // use history
                    rof = pof[-l];
                } else {
                    rof = l;
                    if (l > 32) {       // advance history
                        for (i = WEE_OFHIST - 1; i > 0; i--)
                            pof[i] = pof[i - 1];
                        pof[0] = rof;
                    }
                }
                for (i = 0; i < rle; i++) { // well, copy it !
                    dou[dop + i] = dou[dop + i - rof];
                }
                dop += rle;
            }

            // get new literal length
            lit = wee_dec_len(&rbi, fr6l);
        }

        if (dop >= 2 * WEE_BLK) {       // can we write out stuff
            i = dop - WEE_BLK;
            if (fwrite(dou, 1, i, fout) != i) {
                perror("error writing");
                return 0;
            }
            dop -= i;                   // make space
            memmove(dou, &dou[i], dop);
            osz += i;
        }

        if (rbi.ptr > WEE_BUF) {        // move pointer back, read more
            rbi.max -= rbi.ptr;
            memmove(din, &din[rbi.ptr], rbi.max);
            rbi.ptr = 0;
            i = fread(&din[rbi.max], 1, sizeof(din) - rbi.max, fin);
            if (i == 0) {
                fprintf(stderr, "Unexpected end while reading.\n");
                return 0;
            }
            rbi.max += i;
            isz += i;
        }
    }

    if (fwrite(dou, 1, dop, fout) != dop) {
        perror("error writing");
        return 0;
    }
    osz += dop;

    if (verb) {
        printf("%12zu %12zu  %.1f%%  ",
            isz, osz, 100.0 * ((double) osz - isz) / ((double) osz));
    }

    return osz;
}

