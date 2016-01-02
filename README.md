weesrc
======

Source code for the experimental, compact **wee** file compression utility.

02-Jan-16  Markku-Juhani O. Saarinen  <mjos@iki.fi>

Experimental file compression is based on binary arithmetic coding (BAC),
bigram byte Markov model, and block sorting dictionary search.
Despite these fairly advanced techniques, the codebase is still under 1000
lines and speed is not too bad. This is work in progress.

The adjective (and my file extension) "wee" means *small*, see
[Wiktionary](https://en.wiktionary.org/wiki/wee#Adjective).
I hear the word quite a lot here in Belfast where I live.


**WORK IN PROGRESS, ABSOLUTELY NO WARRANTY WHATSOEVER**

# Usage

A standard `Makefile` is included, which should be workable on most
modern Linux-like boxes.

Command line usage is pretty much standard:
```
Usage: wee [OPTION]... [FILE]...
Compress or uncompress FILEs. OPTIONs:

  -c   Write on standard output, keep original files unchanged.
  -d   Decompress rather than compress files.
  -h   Give this help.
  -k   Keep (don't delete) input files.
  -v   Verbose output.

wee v0.1 by Markku-Juhani O. Saarinen <mjos@iki.fi>  Feedback welcome.
```
You can symlink `unwee` and `weecat` to the
`wee` binary to get corresponding functionality without flags.

# Performance

A test suite based on the
[Cantenbury Compression Corpus](http://corpus.canterbury.ac.nz/) is included.
This first alpha release outperforms *gzip*, but falls little short of the
performance of *xz* (LZMA method) and *bzip2* (block-sorting method).
Just run `make test` to perform full comparison.

Here is the output for *gzip*:
```
gzip           54435  64.2%  alice29.txt
gzip           48951  60.8%  asyoulik.txt
gzip            7999  67.4%  cp.html
gzip            3143  71.8%  fields.c
gzip            1246  66.5%  grammar.lsp
gzip          206779  79.9%  kennedy.xls
gzip          144885  66.0%  lcet10.txt
gzip          195208  59.4%  plrabn12.txt
gzip           56443  89.0%  ptt5
gzip           12924  66.2%  sum
gzip            1756  58.4%  xargs.1
gzip    ============  68.1%  AVERAGE
```

And current version of *wee*:
```
wee 	       53362  64.9%  alice29.txt
wee     	   48578  61.1%  asyoulik.txt
wee 	        7953  67.6%  cp.html
wee 	        3212  71.1%  fields.c
wee 	        1284  65.4%  grammar.lsp
wee     	   67070  93.4%  kennedy.xls
wee 	      134914  68.3%  lcet10.txt
wee     	  186588  61.2%  plrabn12.txt
wee 	       54147  89.4%  ptt5
wee     	   12800  66.5%  sum
wee 	        1798  57.4%  xargs.1
wee		============  69.6%  AVERAGE
```

