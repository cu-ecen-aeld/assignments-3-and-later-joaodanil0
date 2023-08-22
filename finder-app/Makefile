

all: writer

writer: writer.o 
	$(CC) $(LDFLAGS) -o writer writer.o

writer.o: writer.c
	$(CC) $(CCFLAGS) -c writer.c

clean:
	rm -rf *.o
	rm writer