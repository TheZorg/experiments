CC=gcc
CFLAGS=
LDFLAGS=
DEPS=
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=io-test

all: $(SOURCES) $(TARGET)

.ccp.o:
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
