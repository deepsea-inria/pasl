

all: pushpop.exe

pushpop.exe: pushpop.cpp
	g++ -std=gnu++11 -O3 -DNDEBUG -D_GNU_SOURCE -fpermissive -Wno-format -Wfatal-errors -m64 -DTARGET_X86_64 -DTARGET_LINUX -ljemalloc -L../../tools/jemalloc  -o pushpop.exe pushpop.cpp

clean:
	rm -f *.exe