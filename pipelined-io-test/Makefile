CC=g++
CFLAGS=-std=c++11 -ltbb -g -O0
LDFLAGS=-std=c++11 -ltbb -g -O0
#CFLAGS=-g -O2
#LDFLAGS=-g -O2
DEPS=
SOURCES=main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
TARGET=pipelined-io-test

.PHONY: clean

all: $(SOURCES) $(TARGET)

.cpp.o:
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm $(OBJECTS) $(TARGET)
