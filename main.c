// main.c
// 10-Dec-15  Markku-Juhani O. Saarinen <mjos@iki.fi>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <utime.h>

#include "wee.h"

const char wee_usage[] =
    "Usage: wee [OPTION]... [FILE]...\n"
    "Compress or uncompress FILEs. OPTIONs:\n"
    "\n"
    "  -c   Write on standard output, keep original files unchanged.\n"
    "  -d   Decompress rather than compress files.\n"
    "  -h   Give this help.\n"
    "  -k   Keep (don't delete) input files.\n"
    "  -v   Verbose output.\n"
    "\n"
    "wee v0.1 by Markku-Juhani O. Saarinen <mjos@iki.fi>  Feedback welcome.\n";

// command line parameters

int main(int argc, char **argv)
{
    int i, j, fl, er;
    int dec, keep, verb, stdo;
    char fn[4096], *s;
    FILE *fin, *fout;
    struct stat st;
    struct utimbuf ut;

    dec = 0;
    keep = 0;
    verb = 0;
    stdo = 0;

    if (argc > 0) {                     // alternative command names
        s = basename(argv[0]);
        if (strcmp(s, "unwee") == 0)
            dec = 1;
        if (strcmp(s, "weecat") == 0) {
            dec = 1;
            stdo = 1;
        }
    }

    // get parameters
    fl = 0;                             // here used to count actual filenames
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' &&
            !(i > 1 && strcmp(argv[i - 1], "--") == 0)) {

            // command line argument
            for (j = 1; argv[i][j] != 0; j++) {
                switch(argv[i][j]) {

                    case 'c':           // write to stdout
                        stdo = 1;
                        keep = 1;
                        break;

                    case 'd':           // decompress flag
                        dec = 1;
                        break;

                    case 'h':           // version, exit
                        printf("%s", wee_usage);
                        return 0;

                    case 'k':           // keep original files
                        keep = 1;
                        break;

                    case 'v':           // verbose
                        verb = 1;
                        break;


                    case '-':           // either an escape or failure
                        if (j == 1 && argv[i][2] == 0)
                            break;

                    default:
                        fprintf(stderr, "%s: invalid option -- '%c'\n",
                            argv[0], argv[i][j]);
                        return 1;
                }
            }
        } else {
            fl++;
        }
    }

    // no files (or plain "-") -- dump stdin to stdout
    if (fl == 0) {
        if (dec) {
            return wee_file_dec(stdin, stdout, 0) != 0;
        } else {
            return wee_file_enc(stdin, stdout, 0) != 0;
        }
    }

    // now handle files
    fl = 0;                             // used here to count errors
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || (i > 1 && strcmp(argv[i - 1], "--") == 0)) {

            // compose output file name
            j = strlen(argv[i]);
            if (j > sizeof(fn) - 5) {
                fprintf(stderr,
                    "%s: filename too large -- ignored\n", argv[0]);
                fl++;
                continue;
            }

            if (stat(argv[i], &st)) {   // stat it
                fprintf(stderr, "%s: ", argv[0]);
                perror(argv[i]);
                fl++;
                continue;
            }

            if (S_ISDIR(st.st_mode)) {  // check that not directory
                fprintf(stderr, "%s: %s is a directory -- ignored\n",
                    argv[0], argv[i]);
                fl++;
                continue;
            }

            if ((fin = fopen(argv[i], "rb")) == NULL) {
                fprintf(stderr, "%s: ", argv[0]);
                perror(argv[i]);
                fl++;
                continue;
            }

            if (stdo) {                 // -c flag invoked; standard output
                snprintf(fn, sizeof(fn), "standard output");
                fout = stdout;

            } else {                    // normal file naming
                if (dec) {
                    if (j > 4 || strcmp(&argv[i][j - 4], ".wee") == 0) {
                        memcpy(fn, argv[i], j - 4);
                        fn[j - 4] = 0;
                    } else {
                        fprintf(stderr, "%s: %s: Unknown suffix -- ignored.\n",
                            argv[0], argv[i]);
                        fl++;
                        continue;
                    }
                } else {
                    snprintf(fn, sizeof(fn), "%s.wee", argv[i]);
                }

                if ((fout = fopen(fn, "wb")) == NULL) {
                    fprintf(stderr, "%s: ", argv[0]);
                    perror(fn);
                    fclose(fin);
                    fl++;
                    continue;
                }
            }

            if (dec) {                  // transform files
                er = wee_file_dec(fin, fout, verb) == 0;
            } else {
                er = wee_file_enc(fin, fout, verb) == 0;
            }
            fl += er;
            if (verb && er)
                printf("(error)");
            if (verb)                   // add file name to verbose output
                printf("%s\n", fn);

            fclose(fin);                // close files
            if (!stdo) {
                fclose(fout);

                // attempt to change modes and time to match with original
                chmod(fn, st.st_mode & 07777);
                ut.actime = st.st_atime;
                ut.modtime = st.st_mtime;
                utime(fn, &ut);
            }

            if (!keep && er == 0) {     // unless keep is set
                if (remove(argv[i])) {
                    fprintf(stderr, "%s: ", argv[0]);
                    perror(argv[i]);
                    fl++;
                }
            }
        }
    }

    return fl;
}

