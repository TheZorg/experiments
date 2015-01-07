CC=gcc
CFLAGS= -I/home/fabien/git/babeltrace/include `pkg-config --cflags glib-2.0`
LDFLAGS= `pkg-config --libs glib-2.0` -lpapi -lbabeltrace -lbabeltrace-ctf
DEPS=
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=babeltrace-test

all: $(SOURCES) $(TARGET)

.ccp.o:
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
