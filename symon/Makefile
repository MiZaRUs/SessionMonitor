# Makefile for the SyMon
#
PROG	=	symon
OBJS	=	win2utf8.o git.o logcygwin.o config.o logger.o sender.o session.o service.o
#
CC 	=	c:\\cygwin64\\bin\\gcc.exe
#CC 	=	gcc
CFLAGS	=	-std=gnu11 -O2 -fexceptions -W -Wshadow #-pedantic -Werror -Wconversion
LIBS	=	 -lwtsapi32
LINK	=	-Wl,-s

.c.o:
	$(CC) -c $*.c $(CFLAGS) $(IFLAGS)

$(PROG): $(OBJS) 
	$(CC) -o $(PROG) $(OBJS) $(LINK) $(LIBS) $(LDFLAGS)

clean:
	rm -f *.o

install:
	mv -f $(PROG).exe /cygdrive/C/SMonitor/bin/$(PROG).exe
