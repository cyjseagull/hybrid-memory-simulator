hybridsim: 
	g++ -c ptlcall_hybridsim.cpp
	g++ -c util.cpp
	g++ -o ptlcall_hybridsim ptlcall_hybridsim.o util.o

single_flush:
	g++ -o ptlcall_single_flush ptlcall_single_flush.c

nop_test:
	g++ -c nop_test.cpp
	g++ -c util.cpp
	g++ -o nop_test nop_test.o util.o

clean:
	rm -rf *.o
	rm ptlcall_single_flush ptlcall_hybridsim nop_test
