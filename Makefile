
GCC  = gcc
LIBS = -lreadline

shell: clean
	$(CC) main.c json.c -Wall -g3 $(LIBS) -o $@

clean:
	rm -f shell *.o
