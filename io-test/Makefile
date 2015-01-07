CC=gcc
CFLAGS= -fopenmp -g -O2
LDFLAGS= -fopenmp -lpapi -g -O2
DEPS=
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=io-test

.PHONY: clean

all: $(SOURCES) $(TARGET)

.ccp.o:
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm $(OBJECTS) $(TARGET)
