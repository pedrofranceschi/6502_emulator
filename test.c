#include <stdio.h>
#include "cpu.h"

int readFileBytes(const char *name, char **program)
{
    FILE *fl = fopen(name, "r");
    fseek(fl, 0, SEEK_END);
    int program_length = (int)ftell(fl);
    // char *ret = malloc(program_length);
	*program = malloc(program_length);
    fseek(fl, 0, SEEK_SET);
    fread(*program, 1, program_length, fl);
    fclose(fl);
// printf("program_length: %i\n", program_length);
    return program_length;
}

int main(int argc, char *argv[]) {
	// A201860038A00798E903A818A9028501A60165008501860088D0F5
	// const char program[] = { 0xC8, 0x20, 0x00, 0x00 };
	// const char program[] = { 0xA2, 0x01, 0x86, 0x00, 0x38, 0xA0, 0x07, 0x98, 0xE9, 0x03, 0xA8, 0x18, 0xA9, 0x02, 0x85, 0x01, 0xA6, 0x01, 0x65, 0x00, 0x85, 0x01, 0x86, 0x00, 0x88, 0xD0, 0xF5 };

	if(argc != 2) {
		printf("Usage: %s program \n", argv[0]);
		return -1;
	}

	char *program;
	int program_length = readFileBytes(argv[1], &program);

	printf("Program (%i bytes): \n", program_length);

	int i;
	for(i = 0; i < program_length; i++) {
		printf("%i:%02X ", i, (unsigned char)program[i]);
	}

	printf("\n");

	CPU cpu;
	initializeCPU(&cpu);

	cpu.pc = 0x4000;
	writeMemory(&cpu, program, cpu.pc, program_length);
	// cpu.pc += 559; // 700 // 799
	// cpu.pc += 799; // test 06
	// cpu.pc += 1192; // test 09

	// char *buf = malloc(sizeof(char) * 2);
	// buf[0] = 0xC0;
	// buf[1] = 0x01;
	// writeMemory(&cpu, buf, 0x0105, 2);

	// cpu.ps = 0x1;

	// cpu.a = 0x7;
	// cpu.x = 0x2;
	// cpu.y = 0x3;

	// char *buf2 = malloc(sizeof(char) * 2);
	// buf2[0] = 0x08;
	// buf2[1] = 0xEE;
	// writeMemory(&cpu, buf2, 0x0105, 2);

	// printf("cpu->ps: %i\n", cpu.ps);
	// printf("cpu->sp: %i\n", cpu.sp);

	char str[1];
	// int i;

	for(;;) {
		printf("cpu->pc: %i\n", cpu.pc);
		// scanf("%s", str);
		step(&cpu);
		printMemory(&cpu);
		printf("\n\n\n");
		printf("cpu->sp: %x\n", cpu.sp);
		printf("cpu->a: %x\n", cpu.a);
		printf("cpu->x: %x\n", cpu.x);
		printf("cpu->y: %x\n", cpu.y);
		printf("cpu->ps: %x\n\n", cpu.ps);

		if(cpu.pc == 17825) {
			break;
		}
	}

	printMemory(&cpu);

	printf("### results:\n");
	printf("cpu->sp: %x\n", cpu.sp);
	printf("cpu->a: %x\n", cpu.a);
	printf("cpu->x: %x\n", cpu.x);
	printf("cpu->y: %x\n", cpu.y);
	printf("cpu->ps: %x\n", cpu.ps);
	printf("cpu->cycles: %i\n", cpu.cycles);
	// printf("%s\n", );
	// printbitssimple(cpu.ps);	
	printf("MEMORY 9: %x\n", cpu.memory[0x80]);
	printf("MEMORY final: %x\n", cpu.memory[0x0210]);

	freeCPU(&cpu);

	return 0;
}