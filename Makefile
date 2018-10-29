CC = g++
CFLAGS = -c -I/usr/local/include/bamtools -std=c++11
LDFLAGS = /usr/local/lib/libbamtools.a -lpopt -lz
SOURCES = main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = bam-mergeRef

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean: ; rm $(EXECUTABLE) $(OBJECTS) 
