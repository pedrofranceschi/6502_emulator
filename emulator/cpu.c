#include "cpu.h"

// NEVER free program! (TODO memcpy it)
void initializeCPU(CPU *cpu, char *program, int programLength) {
	cpu->memory = malloc(sizeof(unsigned char) * MEMORY_SIZE);
	cpu->program = program;
	cpu->programLength = programLength;
	cpu->cycles = cpu->pc = cpu->sp = cpu->a = cpu->x = cpu->y = cpu->ps = 0; // TODO: move to reset function
}

void writeMemory(CPU *cpu, char *buffer, int start, int offset) {
	int i;
	for(i = 0; i < offset; i++) {
		cpu->memory[start + i] = buffer[i];
	}
}

void readMemory(CPU *cpu, char *buffer, int start, int offset) {
	int i;
	for(i = 0; i < offset; i++) {
		buffer[i] = cpu->memory[start + i];
	}
}

void printMemory(CPU *cpu) {
	int i, j;

	for(i = 0; i < MEMORY_PAGES; i++) {
		printf("=== Page %i\n", i);
		for(j = 0;  j < PAGE_SIZE; j++) {
			printf("%i ", cpu->memory[(i * PAGE_SIZE) + j]);
		}
		printf("\n");
	}
}

void freeCPU(CPU *cpu) {
	free(cpu->memory);
}

void updateStatusFlag(CPU *cpu, char operationResult) { // operationResult can be accumulator, X, Y or any registers
	cpu->ps = (operationResult == 0 ? cpu->ps | 0x2 : cpu->ps & 0xFD ); // sets zero flag (bit 1)
	cpu->ps = (operationResult & 0x80 != 0 ? cpu->ps | 0x80 : cpu-> ps & 0x7F ); // sets negative flag (bit 7 of operationResult is set)
	
	int carry_bit = operationResult & 0x100; // moves bit 8 to carry bit
	cpu->ps = (carry_bit == 0 ? cpu->ps & 0xFE : cpu->ps | 0x1); // updates carry bit (0) on processor status flag
}

int join_bytes(int low_byte, int high_byte) {
	return (high_byte << 0x8) | low_byte;
}

void step(CPU *cpu) { // main code is here
	char currentOpcode = cpu->program[cpu->pc++]; // read program byte number 'program counter' (starting at 0)
	switch(currentOpcode) {
		case 0x00: { // BRK impl
			break;
		}
		case 0x01: { // ORA ind,X
			// cpu->a |= cpu->memory[cpu->program[cpu->pc++]] + cpu->x; // OR with accumulator and memory at (next byte after opcode) + X
			cpu->a |= cpu->memory[cpu->memory[cpu->program[cpu->pc++] + cpu->x]];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 6; // this operation takes 6 cycles;
			break;
		}
		case 0x05: { // ORA zpg
			cpu->a |= cpu->memory[cpu->program[cpu->pc++]]; // OR with memory content at (next byte after opcode in zero page)
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 3;
			break;
		}
		case 0x06: { // ASL zpg
			int zeropage_location = cpu->program[cpu->pc++];
			int zeropage_byte = cpu->memory[zeropage_location];
			zeropage_byte <<= 0x1; // left shift
			
			int carry_bit = zeropage_byte & 0x100; // moves bit 8 to carry bit
			cpu->ps = (carry_bit == 0 ? cpu->ps & 0xFE : cpu->ps | 0x1); // updates carry bit (0) on processor status flag
			
			zeropage_byte &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[zeropage_location] = zeropage_byte;
			updateStatusFlag(cpu, zeropage_byte);
			cpu->cycles += 5;
			
			break;
		}
		case 0x08: { // PHP impl
			break;
		}
		case 0x09: { // ORA immediate
			cpu->a |= cpu->program[cpu->pc++]; // just OR with next byte after opcode
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 2;
			break;
		}
		case 0x0A: { // ASL accumulator
			int accumulator_byte = cpu->a;
			accumulator_byte <<= 0x1; // left shift
			
			int carry_bit = accumulator_byte & 0x100; // moves bit 8 to carry bit
			cpu->ps = (carry_bit == 0 ? cpu->ps & 0xFE : cpu->ps | 0x1); // updates carry bit (0) on processor status flag
			
			accumulator_byte &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->a = accumulator_byte;
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 2;
			
			break;
		}
		case 0x0D: { // ORA abs
			char low_byte = cpu->program[cpu->pc++];
			char high_byte = cpu->program[cpu->pc++];
			int mem_location = join_bytes(low_byte, high_byte);
			
			cpu->a |= cpu->memory[mem_location];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x0E: { // ASL abs
			char low_byte = cpu->program[cpu->pc++];
			char high_byte = cpu->program[cpu->pc++];
			int absolute_address = join_bytes(low_byte, high_byte);
			int address_byte = cpu->memory[absolute_address];
			address_byte <<= 0x1; // left shift
			
			updateStatusFlag(cpu, address_byte);
			address_byte &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[absolute_address] = address_byte;
			cpu->cycles += 6;
			
			break;
		}
		case 0x10: { // BPL rel
			break;
		}
		case 0x11: { // ORA ind,Y
			int mem_initial_location = cpu->memory[cpu->program[cpu->pc++]];
			int mem_final_location = mem_initial_location + cpu->y;
			
			cpu->a |= cpu->memory[mem_final_location];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 5;
			
			int page_boundary = mem_initial_location + (mem_initial_location % PAGE_SIZE);
			
			if(mem_final_location > page_boundary) {
				// page boundary crossed, +1 CPU cycle
				cpu->cycles++;
			}
			
			break;
		}
		case 0x15: { // ORA zpg,X
			int mem_location = cpu->program[cpu->pc++] + cpu->x;
			mem_location &= 0xFF; // removes anything bigger than 0xFF (only 1 byte is allowed)
			
			cpu->a |= cpu->memory[mem_location];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x16: { // ASL zpg,X
			int mem_location = cpu->program[cpu->pc++] + cpu->x;
			mem_location &= 0xFF; // removes anything bigger than 0xFF (only 1 byte is allowed)
			int mem_value = cpu->memory[mem_location];
			mem_value <<= 0x1;
			
			updateStatusFlag(cpu, mem_value);
			mem_value &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[mem_location] = mem_value;
			cpu->cycles += 6;
			
			break;
		}
		case 0x18: { // CLC impl
			break;
		}
		case 0x19:   // ORA abs,Y
		case 0x1D: { // ORA abs,X
			char low_byte = cpu->program[cpu->pc++];
			char high_byte = cpu->program[cpu->pc++];
			int absolute_address = join_bytes(low_byte, high_byte);
			int mem_final_address = absolute_address + (currentOpcode == 0x19 ? cpu->y : cpu->x);
			
			cpu->a |= cpu->memory[mem_final_address];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			int page_boundary = absolute_address + (absolute_address % PAGE_SIZE);
			
			if(mem_final_address > page_boundary) {
				// page boundary crossed, +1 CPU cycle
				cpu->cycles++;
			}
			
			break;
		}
		case 0x1E: { // ASL abs,X
			char low_byte = cpu->program[cpu->pc++];
			char high_byte = cpu->program[cpu->pc++];
			int absolute_address = join_bytes(low_byte, high_byte);
			int mem_final_address = absolute_address + cpu->x;
			
			int mem_value = cpu->memory[mem_final_address];
			mem_value <<= 0x1;
			
			updateStatusFlag(cpu, mem_value);
			mem_value &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[mem_final_address] = mem_value;
			cpu->cycles += 6;
			
			break;
		}
	}
}



int main() {
	const char program[] = { 0x1E, 0x11, 0x10 };
	CPU cpu;
	initializeCPU(&cpu, program, sizeof(program));

	char *buf = malloc(sizeof(char));
	buf[0] = 0x25;
	writeMemory(&cpu, buf, 0x1013, 1);
	free(buf);
	
	cpu.x = 0x2;
	
	printf("cpu->ps: %i\n", cpu.ps);
	
	step(&cpu);

	printMemory(&cpu);
	printf("cpu->a: %i\n", cpu.a);
	printf("cpu->ps: %i\n", cpu.ps);
	printf("cpu->cycles: %i\n", cpu.cycles);
	// printf("%s\n", );
	// printbitssimple(cpu.ps);
	freeCPU(&cpu);

	return 0;
}