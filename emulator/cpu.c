#include "cpu.h"

// NEVER free program! (TODO memcpy it)
void initializeCPU(CPU *cpu, char *program, int programLength) {
	cpu->memory = malloc(sizeof(unsigned char) * MEMORY_SIZE);
	cpu->program = program;
	cpu->programLength = programLength;
	cpu->cycles = cpu->pc = cpu->a = cpu->x = cpu->y = cpu->ps = 0; // TODO: move to reset function
	cpu->sp = 0xFF; // stack pointer starts at 0xFF
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

void pushByteToStack(CPU *cpu, char byte) {
	cpu->memory[0x100 + cpu->sp] = byte;
	cpu->sp--;
}

char pullByteFromStack(CPU *cpu) {
	cpu->sp++;
	return cpu->memory[0x100 + cpu->sp];
}

void updateStatusFlag(CPU *cpu, int operationResult) { // operationResult can be accumulator, X, Y or any result
	cpu->ps = (operationResult == 0 ? cpu->ps | 0x2 : cpu->ps & 0xFD ); // sets zero flag (bit 1)
	cpu->ps = (operationResult & 0x80 != 0 ? cpu->ps | 0x80 : cpu-> ps & 0x7F ); // sets negative flag (bit 7 of operationResult is set)
	
	int carry_bit = operationResult & 0x100; // moves bit 8 to carry bit
	cpu->ps = (carry_bit == 0 ? cpu->ps & 0xFE : cpu->ps | 0x1); // updates carry bit (0) on processor status flag
}

int joinBytes(int low_byte, int high_byte) {
	return (high_byte << 0x8) | low_byte;
}

int calculatePageBoundary(int mem_initial_location, int mem_final_location) {
	// return mem_initial_location + PAGE_SIZE - (mem_initial_location % PAGE_SIZE);
	return mem_initial_location + PAGE_SIZE - 1 - (mem_initial_location % PAGE_SIZE);
}

int addressForIndexedIndirectAddressing(CPU *cpu, char byte) {
	char address_low_byte = byte + cpu->x;
	char address_high_byte = address_low_byte + 1;
	int full_address = joinBytes(cpu->memory[address_low_byte], cpu->memory[address_high_byte]);
	return full_address;
}

int addressForIndirectIndexedAddressing(CPU *cpu, char byte, int *cycles) {
	char operation_low_byte = byte;
	char operation_high_byte = operation_low_byte + 1;
	int full_address = joinBytes(cpu->memory[operation_low_byte], cpu->memory[operation_high_byte]);
	
	if(full_address + cpu->y > calculatePageBoundary(full_address, full_address + cpu->y)) {
		// page boundary crossed, +1 CPU cycle
		(*cycles)++;
	}
	
	return full_address + cpu->y;
}

int addressForZeroPageXAddressing(CPU *cpu, char byte) {	
	return (byte + cpu->x) & 0xFF; // removes anything bigger than 0xFF (only 1 byte is allowed)
}

int addressForZeroPageYAddressing(CPU *cpu, char byte) {	
	return (byte + cpu->y) & 0xFF; // removes anything bigger than 0xFF (only 1 byte is allowed)
}

int rotateByte(CPU *cpu, int byte, int isLeftShift) {	
	byte = (isLeftShift == 1 ? byte << 1 : byte >> 1);
	
	if(cpu->ps & 0x1 != 0) { // carry bit is on
		byte |= 0x1; // turn on bit 0 on operation byte (carry bit shifted on bit 0)
	}
	
	return byte;
}

int addressForAbsoluteAddedAddressing(CPU *cpu, unsigned char low_byte, unsigned char high_byte, char adding, int *cycles) {
	int absolute_address = joinBytes(low_byte, high_byte);
	int mem_final_address = absolute_address + adding;
	
	int page_boundary = calculatePageBoundary(absolute_address, mem_final_address);
	
	if(mem_final_address > page_boundary) {
		// page boundary crossed, +1 CPU cycle
		(*cycles)++;
	}
	
	return mem_final_address;
}

void step(CPU *cpu) { // main code is here
	char currentOpcode = cpu->program[cpu->pc++]; // read program byte number 'program counter' (starting at 0)
	printf("currentOpcode: %i\n", currentOpcode);
	switch(currentOpcode) {
		case 0x00: { // BRK impl
			break;
		}
		case 0x01: { // ORA ind,X
			cpu->a |= cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->program[cpu->pc++])];
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
			
			updateStatusFlag(cpu, zeropage_byte);
			cpu->memory[zeropage_location] = zeropage_byte & 0xFF; // 0xFF removes anything set in a bit > 8;
			cpu->cycles += 5;
			
			break;
		}
		case 0x08: { // PHP impl
			pushByteToStack(cpu, cpu->ps);
			cpu->cycles += 3;
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
			
			updateStatusFlag(cpu, cpu->a);
			cpu->a = accumulator_byte & 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->cycles += 2;
			
			break;
		}
		case 0x0D: { // ORA abs
			char low_byte = cpu->program[cpu->pc++];
			char high_byte = cpu->program[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			
			cpu->a |= cpu->memory[mem_location];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x0E: { // ASL abs
			unsigned char low_byte = cpu->program[cpu->pc++];
			unsigned char high_byte = cpu->program[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
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
			cpu->a |= cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->program[cpu->pc++], &(cpu->cycles))];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 5;
			
			break;
		}
		case 0x15: { // ORA zpg,X			
			cpu->a |= cpu->memory[addressForZeroPageXAddressing(cpu, cpu->program[cpu->pc++])];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x16: { // ASL zpg,X
			int mem_location = addressForZeroPageXAddressing(cpu, cpu->program[cpu->pc++]);
			int mem_value = cpu->memory[mem_location];
			mem_value <<= 0x1;
			
			updateStatusFlag(cpu, mem_value);
			mem_value &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[mem_location] = mem_value;
			cpu->cycles += 6;
			
			break;
		}
		case 0x18: { // CLC impl
			cpu->ps = cpu->ps & 0xFE; // clean carry bit (0)
			cpu->cycles += 2;
			
			break;
		}
		case 0x19:   // ORA abs,Y
		case 0x1D: { // ORA abs,X
			unsigned char low_byte = cpu->program[cpu->pc++];
			unsigned char high_byte = cpu->program[cpu->pc++];
			
			cpu->a |= cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0x39 ? cpu->y : cpu->x), &cpu->cycles)];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x1E: { // ASL abs,X
			unsigned char low_byte = cpu->program[cpu->pc++];
			unsigned char high_byte = cpu->program[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			int mem_final_address = absolute_address + cpu->x;
			
			int mem_value = cpu->memory[mem_final_address];
			mem_value <<= 0x1;
			
			updateStatusFlag(cpu, mem_value);
			mem_value &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[mem_final_address] = mem_value;
			cpu->cycles += 6;
			
			break;
		}
		case 0x20: { // JSR abs
			unsigned char low_byte = cpu->program[cpu->pc++];
			unsigned char high_byte = cpu->program[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			
			pushByteToStack(cpu, cpu->pc - 1); // push (program counter - 1) to stack
			cpu->pc = absolute_address;
			cpu->cycles += 6;
			
			break;
		}
		case 0x21: { // AND ind,X
			cpu->a &= cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->program[cpu->pc++])];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 6; // this operation takes 6 cycles;
			
			break;
		}
		case 0x24: { // BIT zpg
			break;
		}
		case 0x25: { // AND zpg
			cpu->a &= cpu->memory[cpu->program[cpu->pc++]]; // AND with memory content at (next byte after opcode in zero page)
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 3;
			break;
		}
		case 0x26: { // ROL zpg
			char zeropage_location = cpu->program[cpu->pc++];
			int operation_byte = rotateByte(cpu, cpu->memory[zeropage_location], 1);
			
			updateStatusFlag(cpu, operation_byte);
			cpu->memory[zeropage_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 5;
			
			break;
		}
		case 0x28: { // PLP impl
			break;
		}
		case 0x29: { // AND immediate
			cpu->a &= cpu->program[cpu->pc++]; // just OR with next byte after opcode
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 2;
			break;
		}
		case 0x2A: { // ROL accumulator
			int operation_byte = rotateByte(cpu, cpu->a, 1);
			
			updateStatusFlag(cpu, operation_byte);
			cpu->a = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 2;
			
			break;
		}
		case 0x2C: { // BIT abs
			break;
		}
		case 0x2D: { // AND abs
			char low_byte = cpu->program[cpu->pc++];
			char high_byte = cpu->program[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			
			cpu->a &= cpu->memory[mem_location];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x2E: { // ROL abs
			unsigned char low_byte = cpu->program[cpu->pc++];
			unsigned char high_byte = cpu->program[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			int operation_byte = rotateByte(cpu, cpu->memory[mem_location], 1);
			
			updateStatusFlag(cpu, operation_byte);
			cpu->memory[mem_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 6;
			
			break;
		}
		case 0x30: { // BMI rel
			break;
		}
		case 0x31: { // AND ind,Y
			cpu->a &= cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->program[cpu->pc++], &(cpu->cycles))];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 5;
			
			break;
		}
		case 0x35: { // AND zpg,X			
			cpu->a &= cpu->memory[addressForZeroPageXAddressing(cpu, cpu->program[cpu->pc++])];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x36: { // ROL zpg,X
			int zeropage_location = addressForZeroPageXAddressing(cpu, cpu->program[cpu->pc++]);
			int operation_byte = rotateByte(cpu, cpu->memory[zeropage_location], 1);
			
			updateStatusFlag(cpu, operation_byte);
			cpu->memory[zeropage_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 6;
			
			break;
		}
		case 0x38: { // SEC impl
			break;
		}
		case 0x39:   // AND abs,Y
		case 0x3D: { // AND abs,X
			unsigned char low_byte = cpu->program[cpu->pc++];
			unsigned char high_byte = cpu->program[cpu->pc++];
			
			cpu->a &= cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0x39 ? cpu->y : cpu->x), &cpu->cycles)];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
		case 0x3E: { // ROL abs,X
			unsigned char low_byte = cpu->program[cpu->pc++];
			unsigned char high_byte = cpu->program[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			int mem_final_address = absolute_address + cpu->x;
			int operation_byte = rotateByte(cpu, cpu->memory[mem_final_address], 1);
			
			updateStatusFlag(cpu, operation_byte);
			cpu->memory[mem_final_address] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 7;

			break;
		}
		case 0x40: { // RTI impl
			break;
		}
		case 0x41: { // EOR ind,X
			cpu->a ^= cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->program[cpu->pc++])];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 6; // this operation takes 6 cycles;
			break;
		}
		case 0x45: { // EOR zpg
			cpu->a ^= cpu->memory[cpu->program[cpu->pc++]];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 3;
		}
		case 0x46: { // LSR zpg
			break;
		}
		case 0x48: { // PHA impl
			break;
		}
		case 0x49: { // EOR immediate
			cpu->a ^= cpu->program[cpu->pc++]; // just OR with next byte after opcode
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 2;
			break;
		}
		case 0x4A: { // LSR accumulator
			break;
		}
		case 0x4C: { // JMP abs
			break;
		}
		case 0x4D: { // EOR abs
			char low_byte = cpu->program[cpu->pc++];
			char high_byte = cpu->program[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			
			cpu->a ^= cpu->memory[mem_location];
			updateStatusFlag(cpu, cpu->a);
			cpu->cycles += 4;
			
			break;
		}
	}
}

int main() {
	const char program[] = { 0x4D, 0x07, 0x03 };
	CPU cpu;
	initializeCPU(&cpu, program, sizeof(program));

	char *buf = malloc(sizeof(char) * 2);
	buf[0] = 0x0;
	buf[1] = 0x49;
	writeMemory(&cpu, buf, 0x0306, 2);
	
	cpu.a = 0x17;
	// cpu.x = 0x2;
	
	// char *buf2 = malloc(sizeof(char));
	// buf2[0] = 0x27;
	// writeMemory(&cpu, buf2, 0x0249, 1);
	// free(buf2);
	
	printf("cpu->ps: %i\n", cpu.ps);
	printf("cpu->sp: %i\n", cpu.sp);
	
	while(cpu.pc < sizeof(program)) {
		step(&cpu);
	}

	printMemory(&cpu);
	printf("cpu->sp: %i\n", cpu.sp);
	printf("cpu->a: %i\n", cpu.a);
	printf("cpu->ps: %i\n", cpu.ps);
	printf("cpu->cycles: %i\n", cpu.cycles);
	// printf("%s\n", );
	// printbitssimple(cpu.ps);
	freeCPU(&cpu);

	return 0;
}