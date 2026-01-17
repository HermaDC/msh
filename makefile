CC = gcc
CFLAGS = -Wall -g
OBJ = main.o built_ins.o
VERSION = 0.0.1
#TODO arrelar esto

# Objetivo principal: genera el siguiente ejecutable
all: $(VERSION_FILE) $(OBJ)
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -o msh_v$(VERSION) $(OBJ)

# Archivos objeto
main.o: main.c built_ins.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c main.c -o main.o

built_ins.o: built_ins.c built_ins.h
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c built_ins.c -o built_ins.o

# Limpiar todos los archivos compilados
clean:
	rm -f *.o 