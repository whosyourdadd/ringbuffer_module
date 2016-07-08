# Simplistic Makefile for malloc_count example
PROJECT = v2_ringbuffer
CC = gcc
CFLAGS = -g -W -Wall -pthread
LIBS = -ldl
HEADER_FILE = .
CFLAGS += -I$(HEADER_FILE)
#SOURCES = ringbuffer.c
SOURCES = $(PROJECT).c
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
all: $(PROJECT)

%.o: %.c $(HEADER_FILE)
	$(CC) -c $< -o $@ $(LIBS) $(CFLAGS)

	$(PROJECT): $(OBJECTS)
		$(CC) $^ -o $@ $(LIBS) $(CFLAGS)

.PHONY: clean
clean:
	rm -rf $(PROJECT)
	rm -rf *.dSYM
	rm -rf *.data.old
	rm -rf *.log
	find .. -name "*.o" -exec rm {} \;