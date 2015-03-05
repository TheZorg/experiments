CC=gcc
CFLAGS=
LDFLAGS= -lpthread
DEPS=
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=migrate-test

.PHONY: clean

all: $(SOURCES) $(TARGET)

.ccp.o:
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm $(OBJECTS) $(TARGET)
