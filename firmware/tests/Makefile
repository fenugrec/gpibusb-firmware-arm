CC = gcc
BASICFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic
OPTFLAGS = -g
CFLAGS = $(BASICFLAGS) $(OPTFLAGS) $(EXFLAGS)

TGTLIST = hash cmdstring

all: $(TGTLIST)


hash:	hash.c ../cmd_hashtable.c

cmdstring:	cmdstring.c

clean:
	rm -f *.o
	rm -f $(TGTLIST)
