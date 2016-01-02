# Makefile
# 03-Dec-15 Markku-Juhani O. Saarinen <mjos@iki.fi>

DIST	= weesrc
BIN	= wee
OBJS	= aric.o weef.o main.o

CC	= gcc
CFLAGS	= -Wall -Ofast -march=native
LIBS	=
LDFLAGS	=
INCS	=

$(BIN):	$(OBJS)
	$(CC) $(LDFLAGS) -o $(BIN) $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

clean:
	rm -rf $(DIST)-*.t?z $(OBJS) $(BIN)

test:	$(BIN)
	cd corpus; bash compare.sh

dist:	clean
	cd ..; \
	tar cfvJ $(DIST)/$(DIST)-`date -u "+%Y%m%d%H%M00"`.txz $(DIST)/*
