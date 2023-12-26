CC = clang
CFLAGS = -Wall -lutil


nscw: main.c
	$(CC) $(CFLAGS) -o nscw main.c

debug:
	$(CC) -g $(CFLAGS) -o nscw main.c

clean:
	rm nscw
