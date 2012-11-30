#include <stdio.h>
// #include "memory.h"

#define PAGE_SIZE 256
#define MEMORY_PAGES 256
#define MEMORY_SIZE MEMORY_PAGES * PAGE_SIZE

typedef struct {
	char *memory;
	char *program;
	int programLength;
	
	// REGISTERS
	int pc; // program counter is two bytes
	char sp, a, x, y, ps; // stack pointer, accumulator, x register, y register, processor status flag;	
	
	// processor status flags:
	// N V - B D I Z C
	// N = negative flag
	// V = overflow flag
	// B = break flag
	// D = decimal mode flag
	// I = interrupt disabled flag
	// Z = zero flag
	// C = carry flag
} CPU;