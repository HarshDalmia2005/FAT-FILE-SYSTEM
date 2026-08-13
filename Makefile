CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2
LDFLAGS = -pthread

SOURCES = main.cpp Disk.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = vfs_test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET) virtual_disk.bin
