CC = gcc
CFLAGS = -g -O2 -Wall
LFLAGS =

all:	webserver updated_webserver

webserver:	webserver.o
	$(CC) $(LFLAGS) -o webserver webserver.o

webserver.o:	webserver.c
	$(CC) $(CFLAGS) -c webserver.c

updated_webserver: updated_webserver.o
	$(CC) $(LFLAGS) -o updated_webserver updated_webserver.o

updated_webserver.o: updated_webserver.c
	$(CC) $(CFLAGS) -c updated_webserver.c

submit:
	tar -czvf hw2_submit.tar.gz README Makefile webserver.c updated_webserver.c test.html

clean:
	rm -f *~ *.o webserver updated_webserver
