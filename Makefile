CC      = gcc
CFLAGS  = -pg -c -pedantic -ansi -O -Wall -DDEBUG
LD      = gcc
LDFLAGS = -pg -o $(PROGRAM)
PROGRAM = proxy
CSRCS   = main.c process0.c process1.c
OBJS    = $(CSRCS:.c=.o)
LIBS    =

.c.o:
	$(CC) $(CFLAGS) $<

$(PROGRAM): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS)

clean: ;
	rm $(OBJS) $(PROGRAM)
