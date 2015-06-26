# Makefile for the SMonitor
#
PROG	=	SMonitor
OBJS	=	logger.o utf2win.o tray_icon.o icons.o ipc.o window.o
#
CC 	=	c:\\cygwin64\\bin\\gcc.exe
CFLAGS	=	-O2 -W -Wshadow #-pedantic -Werror -Wconversion
LIBS	=	-lgdi32
LINK	=	-Wl,--no-whole-archive

.c.o:
	$(CC) -c $*.c $(CFLAGS) $(IFLAGS)

$(PROG): $(OBJS) 
	$(CC) -o $(PROG) $(OBJS) $(LINK) $(LIBS) $(LDFLAGS)

clean:
	rm -f logger.o
	rm -f utf2win.o
	rm -f window.o
	rm -f tray_icon.o
	rm -f ipc.o
	rm -f main.o

install:
	mv -f $(PROG).exe /cygdrive/C/SMonitor/bin/$(PROG).exe
