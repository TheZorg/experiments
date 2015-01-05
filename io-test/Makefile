CC=gcc
CFLAGS= -fopenmp
LDFLAGS= -fopenmp -lpapi
DEPS=
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=io-test

all: $(SOURCES) $(TARGET)

.ccp.o:
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
