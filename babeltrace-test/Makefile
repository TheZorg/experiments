CC=gcc
CFLAGS= -I../contrib/babeltrace/include `pkg-config --cflags glib-2.0` -g
LDFLAGS= `pkg-config --libs glib-2.0` -lpapi -lbabeltrace -lbabeltrace-ctf -g
DEPS=
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=babeltrace-test

.PHONY: clean

all: $(SOURCES) $(TARGET)

.ccp.o:
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm $(OBJECTS) $(TARGET)
