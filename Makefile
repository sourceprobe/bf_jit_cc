bf.jit.o: bf.jit.cc bf.jit.s mm.h
	g++ bf.jit.s bf.jit.cc -no-pie -std=gnu++17 -O3 -masm=intel -o bf.jit.o

