CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2
LDFLAGS = -pthread

CORE_OBJS = Disk.o FAT.o VFS.o

all: vfs_test vfs_server vfs_client

vfs_test: main.o $(CORE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

vfs_server: server.o $(CORE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

vfs_client: cli_client.o
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f *.o vfs_test vfs_server vfs_client virtual_disk.bin
