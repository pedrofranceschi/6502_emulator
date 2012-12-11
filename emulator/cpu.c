#include "cpu.h"

// NEVER free program! (TODO memcpy it)
void initializeCPU(CPU *cpu) {
	cpu->memory = malloc(sizeof(unsigned char) * MEMORY_SIZE);
	cpu->cycles = cpu->pc = cpu->a = cpu->x = cpu->y = 0; // TODO: move to reset function
	cpu->ps = 0x4; // interrupt disabled is on
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
			printf("%x ", cpu->memory[(i * PAGE_SIZE) + j]);
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

void updateStatusRegister(CPU *cpu, int operationResult, char ignore_bits) { // operationResult can be accumulator, X, Y or any result
	if((ignore_bits & 0x2) == 0) { // if is not ignoring bit 1
		cpu->ps = ((operationResult == 0) ? cpu->ps | 0x2 : cpu->ps & 0xFD ); // sets zero flag (bit 1)
	}
	
	if((ignore_bits & 0x80) == 0) {  // if is not ignoring bit 7
		cpu->ps = ((operationResult & 0x80) != 0 ? cpu->ps | 0x80 : cpu->ps & 0x7F ); // sets negative flag (bit 7 of operationResult is set)
	}
	
	if((ignore_bits & 0x1) == 0) {  // if is not ignoring bit 0
		cpu->ps = ((operationResult >> 0x8) == 0 ? cpu->ps & 0xFE : cpu->ps | 0x1); // updates carry bit (0) on processor status flag
	}
}

int joinBytes(int low_byte, int high_byte) {
	return (high_byte << 0x8) | low_byte;
}

int calculatePageBoundary(int mem_initial_location, int mem_final_location) {
	// return mem_initial_location + PAGE_SIZE - (mem_initial_location % PAGE_SIZE);
	return mem_initial_location + PAGE_SIZE - 1 - (mem_initial_location % PAGE_SIZE);
}

int addressForIndexedIndirectAddressing(CPU *cpu, char byte) {
	unsigned char address_low_byte = byte + cpu->x;
	unsigned char address_high_byte = address_low_byte + 1;
	int full_address = joinBytes(cpu->memory[address_low_byte], cpu->memory[address_high_byte]);
	return full_address;
}

int addressForIndirectIndexedAddressing(CPU *cpu, char byte, int *cycles) {
	unsigned char operation_low_byte = byte;
	unsigned char operation_high_byte = operation_low_byte + 1;
	int full_address = joinBytes(cpu->memory[operation_low_byte], cpu->memory[operation_high_byte]);
	
	if(full_address + cpu->y > calculatePageBoundary(full_address, full_address + cpu->y) && cycles != NULL) {
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
	if(isLeftShift == 1) {
		byte <<= 1;
		
		if(cpu->ps & 0x1 != 0) { // carry bit is on
			byte |= 0x1; // turn on bit 0 on operation byte (carry bit shifted on bit 0)
		}
	} else {
		int pre_ps = cpu->ps;
		
		if(byte & 0x1 != 0) {
			cpu->ps |= 0x1;
		}
		
		byte >>= 1;
		
		if(pre_ps & 0x1 != 0) {
			byte |= 0x80;
		}
	}
	
	return byte;
}

int addressForAbsoluteAddedAddressing(CPU *cpu, unsigned char low_byte, unsigned char high_byte, char adding, int *cycles) {
	int absolute_address = joinBytes(low_byte, high_byte);
	int mem_final_address = absolute_address + adding;
	
	int page_boundary = calculatePageBoundary(absolute_address, mem_final_address);
	
	if(mem_final_address > page_boundary && cycles != NULL) {
		// page boundary crossed, +1 CPU cycle
		(*cycles)++;
	}
	
	return mem_final_address;
}

int logicalShiftRight(CPU *cpu, int operation_byte) {
	if((operation_byte & 0x1) != 0) { // if bit 0 is on
		cpu->ps |= 0x1; // turn on bit 0 (carry bit)
	}
	
	return operation_byte >> 0x1;
}

void addWithCarry(CPU *cpu, int operation_byte) {
	int accumulator_with_carry = ((cpu->ps & 0x1) != 0 ? (cpu->a | 0x100) : cpu->a); // if carry bit is on, (accumulator + carry) = accumulator with bit 8 on
	int result = accumulator_with_carry + operation_byte;
	
	cpu->ps = ((result >> 0x8) == 0 ? cpu->ps & 0xFE : cpu->ps | 0x1); // updates carry bit (0) on processor status flag
	
	int complement_result = (char)accumulator_with_carry + (char)operation_byte;
	
	if(complement_result < -128 || complement_result > 127) { // overflow detection
		cpu->ps |= 0x40; // set overflow bit on (bit 6)
	}
	
	cpu->a = result & 0xFF; // just get first 8 bits
}

void subtractWithCarry(CPU *cpu, int operation_byte) {
	int accumulator_with_not_carry = ((cpu->ps & 0x1) == 0 ? (cpu->a | 0x100) : cpu->a); // if carry bit is off, (accumulator + carry) = accumulator with bit 8 on
	int result = accumulator_with_not_carry - operation_byte;
	
	cpu->ps = ((result >> 0x8) == 0 ? cpu->ps & 0xFE : cpu->ps | 0x1); // updates carry bit (0) on processor status flag
	
	int complement_result = (char)accumulator_with_not_carry + (char)operation_byte;
	
	if(complement_result < -128 || complement_result > 127) { // overflow detection
		cpu->ps |= 0x40; // set overflow bit on (bit 6)
	}
	
	cpu->a = result & 0xFF; // just get first 8 bits
}

void compareBytes(CPU *cpu, unsigned char byte1, unsigned char byte2) {
	cpu->ps = ((byte1 >= byte2) ? cpu->ps | 0x01 : cpu->ps & 0xFE); // updates carry bit (0) on processor status flag
	cpu->ps = ((byte1 == byte2) ? cpu->ps | 0x02 : cpu->ps & 0xFD); // updates zero bit (0) on processor status flag
	updateStatusRegister(cpu, byte1 - byte2, 0x3); // 0x3 = ignores zero and carry bits
}

void testByte(CPU *cpu, char byte) {
	cpu->ps = ((byte & 0x40) != 0 ? cpu->ps | 0x40 : cpu->ps & 0xBF ); // if bit 6 is on on byte... turn bit 6 on on processor status (and vice-versa)
	cpu->ps = ((byte & 0x80) != 0 ? cpu->ps | 0x80 : cpu->ps & 0x7F ); // if bit 7 is on on byte... turn bit 7 on on processor status (and vice-versa)
	updateStatusRegister(cpu, (byte & cpu->a), 0xFD); // 0xFD = ignores all bits except bit 2 (just updates zero flag)
}

void branchToRelativeAddressIf(CPU *cpu, char relative_address, int condition) {
	if(!condition) return;
	
	int branch_location = cpu->pc + relative_address;
	int page_boundary = calculatePageBoundary(cpu->pc, branch_location);
	cpu->pc = branch_location;
	
	if(branch_location > page_boundary) {
		// branch to different page
		cpu->cycles += 2;
	} else {
		// branch to same page
		cpu->cycles += 1;
	}
}

void step(CPU *cpu) { // main code is here
	unsigned char currentOpcode = cpu->memory[cpu->pc++]; // read program byte number 'program counter' (starting at 0)
	printf("currentOpcode: %x\n", currentOpcode);
	switch(currentOpcode) {
		case 0x00: { // BRK impl
			printf("BREAK. \n");
			exit(0);
			break;
		}
		case 0x01: { // ORA ind,X
			cpu->a |= cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 6; // this operation takes 6 cycles;
			
			break;
		}
		case 0x05: { // ORA zpg
			cpu->a |= cpu->memory[cpu->memory[cpu->pc++]]; // OR with memory content at (next byte after opcode in zero page)
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 3;
			
			break;
		}
		case 0x06: { // ASL zpg
			int zeropage_location = cpu->memory[cpu->pc++];
			int zeropage_byte = cpu->memory[zeropage_location];
			zeropage_byte <<= 0x1; // left shift
			
			updateStatusRegister(cpu, zeropage_byte, 0);
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
			cpu->a |= cpu->memory[cpu->pc++]; // just OR with next byte after opcode
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 2;
			break;
		}
		case 0x0A: { // ASL accumulator
			int accumulator_byte = cpu->a;
			accumulator_byte <<= 0x1; // left shift
			
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->a = accumulator_byte & 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->cycles += 2;
			
			break;
		}
		case 0x0D: { // ORA abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			
			cpu->a |= cpu->memory[mem_location];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x0E: { // ASL abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			int address_byte = cpu->memory[absolute_address];
			address_byte <<= 0x1; // left shift
			
			updateStatusRegister(cpu, address_byte, 0);
			address_byte &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[absolute_address] = address_byte;
			cpu->cycles += 6;
			
			break;
		}
		case 0x10: { // BPL rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x80) == 0)); // if bit 7 is off
			cpu->cycles += 2;
			
			break;
		}
		case 0x11: { // ORA ind,Y
			cpu->a |= cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 5;
			
			break;
		}
		case 0x15: { // ORA zpg,X			
			cpu->a |= cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x16: { // ASL zpg,X
			int mem_location = addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++]);
			int mem_value = cpu->memory[mem_location];
			mem_value <<= 0x1;
			
			updateStatusRegister(cpu, mem_value, 0);
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
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			
			cpu->a |= cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0x19 ? cpu->y : cpu->x), &cpu->cycles)];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x1E: { // ASL abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			int mem_final_address = absolute_address + cpu->x;
			
			int mem_value = cpu->memory[mem_final_address];
			mem_value <<= 0x1;
			
			updateStatusRegister(cpu, mem_value, 0);
			mem_value &= 0xFF; // 0xFF removes anything set in a bit > 8
			cpu->memory[mem_final_address] = mem_value;
			cpu->cycles += 6;
			
			break;
		}
		case 0x20: { // JSR abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			
			int program_counter = cpu->pc - 1;
			pushByteToStack(cpu, program_counter >> 0x8); // push second byte of program counter on stack
			pushByteToStack(cpu, program_counter & 0xFF); // push first byte of program counter on stack
			cpu->pc = absolute_address;
			cpu->cycles += 6;
			
			break;
		}
		case 0x21: { // AND ind,X
			cpu->a &= cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 6; // this operation takes 6 cycles;
			
			break;
		}
		case 0x24: { // BIT zpg
			testByte(cpu, cpu->memory[cpu->memory[cpu->pc++]]);
			cpu->cycles += 3;
			
			break;
		}
		case 0x25: { // AND zpg
			cpu->a &= cpu->memory[cpu->memory[cpu->pc++]]; // AND with memory content at (next byte after opcode in zero page)
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 3;
			
			break;
		}
		case 0x26: { // ROL zpg
			unsigned char zeropage_location = cpu->memory[cpu->pc++];
			int operation_byte = rotateByte(cpu, cpu->memory[zeropage_location], 1);
			
			updateStatusRegister(cpu, operation_byte, 0);
			cpu->memory[zeropage_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 5;
			
			break;
		}
		case 0x28: { // PLP impl
			cpu->ps = pullByteFromStack(cpu);
			cpu->cycles += 4;
			
			break;
		}
		case 0x29: { // AND immediate
			cpu->a &= cpu->memory[cpu->pc++]; // just OR with next byte after opcode
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 2;
			break;
		}
		case 0x2A: { // ROL accumulator
			int operation_byte = rotateByte(cpu, cpu->a, 1);
			
			updateStatusRegister(cpu, operation_byte, 0);
			cpu->a = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 2;
			
			break;
		}
		case 0x2C: { // BIT abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			
			testByte(cpu, cpu->memory[mem_location]);
			cpu->cycles += 4;
			
			break;
		}
		case 0x2D: { // AND abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			
			cpu->a &= cpu->memory[mem_location];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x2E: { // ROL abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			int operation_byte = rotateByte(cpu, cpu->memory[mem_location], 1);
			
			updateStatusRegister(cpu, operation_byte, 0);
			cpu->memory[mem_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 6;
			
			break;
		}
		case 0x30: { // BMI rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x80) != 0)); // if bit 7 is on
			cpu->cycles += 2;
			
			break;
		}
		case 0x31: { // AND ind,Y
			cpu->a &= cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 5;
			
			break;
		}
		case 0x35: { // AND zpg,X			
			cpu->a &= cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x36: { // ROL zpg,X
			int zeropage_location = addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++]);
			int operation_byte = rotateByte(cpu, cpu->memory[zeropage_location], 1);
			
			updateStatusRegister(cpu, operation_byte, 0);
			cpu->memory[zeropage_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 6;
			
			break;
		}
		case 0x38: { // SEC impl
			cpu->ps |= 0x1; // turn carry bit (0) on
			cpu->cycles += 2;
			
			break;
		}
		case 0x39:   // AND abs,Y
		case 0x3D: { // AND abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			
			cpu->a &= cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0x39 ? cpu->y : cpu->x), &cpu->cycles)];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x3E: { // ROL abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			int mem_final_address = absolute_address + cpu->x;
			int operation_byte = rotateByte(cpu, cpu->memory[mem_final_address], 1);
			
			updateStatusRegister(cpu, operation_byte, 0);
			cpu->memory[mem_final_address] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 7;

			break;
		}
		case 0x40: { // RTI impl
			cpu->ps = pullByteFromStack(cpu);
			unsigned char low_byte = pullByteFromStack(cpu); // pull second byte of program counter on stack
			unsigned char high_byte = pullByteFromStack(cpu); // pull first byte of program counter on stack
			int absolute_address = joinBytes(low_byte, high_byte) + 1;
			cpu->pc = absolute_address;
			cpu->cycles += 6;
			
			break;
		}
		case 0x41: { // EOR ind,X
			cpu->a ^= cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 6; // this operation takes 6 cycles;
			
			break;
		}
		case 0x45: { // EOR zpg
			cpu->a ^= cpu->memory[cpu->memory[cpu->pc++]];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 3;
			
			break;
		}
		case 0x46: { // LSR zpg
			unsigned char zeropage_location = cpu->memory[cpu->pc++];
			int operation_byte = logicalShiftRight(cpu, cpu->memory[zeropage_location]);
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			
			cpu->memory[zeropage_location] = operation_byte;
			cpu->cycles += 5;
			
			break;
		}
		case 0x48: { // PHA impl
			pushByteToStack(cpu, cpu->a);
			cpu->cycles += 3;
			
			break;
		}
		case 0x49: { // EOR immediate
			cpu->a ^= cpu->memory[cpu->pc++]; // just OR with next byte after opcode
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 2;
			break;
		}
		case 0x4A: { // LSR accumulator
			int operation_byte = logicalShiftRight(cpu, cpu->a);
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			
			cpu->a = operation_byte;
			cpu->cycles += 2;
			
			break;
		}
		case 0x4C: { // JMP abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			
			printf("jumping to %x %x \n", low_byte, high_byte);
			
			int mem_location = joinBytes(low_byte, high_byte);
			
			printf("memory: %x\n", mem_location);
			
			cpu->pc = mem_location;
			cpu->cycles += 3;
			
			break;
		}
		case 0x4D: { // EOR abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			
			cpu->a ^= cpu->memory[mem_location];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x4E: { // LSR abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			int operation_byte = logicalShiftRight(cpu, cpu->memory[mem_location]);
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			
			cpu->memory[mem_location] = operation_byte;
			cpu->cycles += 6;
			
			break;
		}
		case 0x50: { // BVC rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x40) != 0)); // if bit 6 is on
			cpu->cycles += 2;
			
			break;
		}
		case 0x51: { // EOR ind,Y
			cpu->a ^= cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 5;
			
			break;
		}
		case 0x55: { // EOR zpg,X
			cpu->a ^= cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x56: { // LSR zpg,X
			int mem_location = addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++]);
			int operation_byte = logicalShiftRight(cpu, cpu->memory[mem_location]);
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			
			cpu->memory[mem_location] = operation_byte;
			cpu->cycles += 6;
			
			break;
		}
		case 0x58: { // CLI impl
			cpu->ps &= 0xFB; // turn interrupt bit (3) off
			cpu->cycles += 2;
			
			break;
		}
		case 0x59:   // EOR abs,Y
		case 0x5D: { // EOR abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			
			cpu->a ^= cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0x59 ? cpu->y : cpu->x), &cpu->cycles)];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0x5E: { // LSR abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			
			int mem_location = addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->x, NULL);
			int operation_byte = logicalShiftRight(cpu, cpu->memory[mem_location]);
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			
			cpu->memory[mem_location] = operation_byte;
			cpu->cycles += 7;
			
			break;
		}
		case 0x60: { // RTS impl
			unsigned char low_byte = pullByteFromStack(cpu); // pull second byte of program counter on stack
			unsigned char high_byte = pullByteFromStack(cpu); // pull first byte of program counter on stack
			int absolute_address = joinBytes(low_byte, high_byte) + 1;
			cpu->pc = absolute_address;
			cpu->cycles += 6;
			
			break;
		}
		case 0x61: { // ADC ind,X
			int operation_byte = cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])];
			addWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 6;
			
			break;
		}
		case 0x65: { // ADC zpg
			int operation_byte = cpu->memory[cpu->memory[cpu->pc++]];
			addWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 3;
			
			break;
		}
		case 0x66: { // ROR zpg
			unsigned char zeropage_location = cpu->memory[cpu->pc++];
			int operation_byte = rotateByte(cpu, cpu->memory[zeropage_location], 0);
			
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->memory[zeropage_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 5;
			
			break;
		}
		case 0x68: { // PLA impl
			cpu->a = pullByteFromStack(cpu);
			cpu->cycles += 4;
			
			break;
		}
		case 0x69: { // ADC immediate
			int operation_byte = cpu->memory[cpu->pc++];
			addWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 2;
			
			break;
		}
		case 0x6A: { // ROR accumulator
			int operation_byte = rotateByte(cpu, cpu->a, 0);
			
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->a = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 2;
			
			break;
		}
		case 0x6C: { // JMP ind
			int indirect_mem_location = joinBytes(cpu->memory[cpu->pc++], cpu->memory[cpu->pc++]);
			unsigned char low_byte = cpu->memory[indirect_mem_location];
			unsigned char high_byte = cpu->memory[indirect_mem_location + 1];
			int real_memory_location = joinBytes(low_byte, high_byte);
			
			cpu->pc = real_memory_location;
			cpu->cycles += 5;
			
			break;
		}
		case 0x6D: { // ADC abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);			
			int operation_byte = cpu->memory[mem_location];
			
			addWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 4;
			
			break;
		}
		case 0x6E: { // ROR abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			int operation_byte = rotateByte(cpu, cpu->memory[mem_location], 0);
			
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->memory[mem_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 6;
			
			break;
		}
		case 0x70: { // BVS rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x40) == 0)); // if bit 6 is off
			cpu->cycles += 2;
			
			break;
		}
		case 0x71: { // ADC ind,Y
			int operation_byte = cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], &(cpu->cycles))];
			addWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 5;
			
			break;
		}
		case 0x75: { // ADC zpg,X
			int operation_byte = cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])];
			addWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 4;
			
			break;
		}
		case 0x76: { // ROR zpg,X
			int zeropage_location = addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++]);
			int operation_byte = rotateByte(cpu, cpu->memory[zeropage_location], 0);
			
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->memory[zeropage_location] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 6;
			
			break;
		}
		case 0x78: { // SEI impl
			cpu->ps |= 0x4; // set interrupt disable bit (2) on
			cpu->cycles += 2;
			
			break;
		}
		case 0x79:   // ADC abs,Y
		case 0x7D: { // ADC abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int operation_byte = cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0x79 ? cpu->y : cpu->x), &cpu->cycles)];
			
			addWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 4;
			
			break;
		}
		case 0x7E: { // ROR abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int absolute_address = joinBytes(low_byte, high_byte);
			int mem_final_address = absolute_address + cpu->x;
			int operation_byte = rotateByte(cpu, cpu->memory[mem_final_address], 0);
			
			updateStatusRegister(cpu, operation_byte, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->memory[mem_final_address] = operation_byte & 0xFF; // 0xFF removes anything set in bit > 8
			cpu->cycles += 7;

			break;
		}
		case 0x81: { // STA ind,X
			cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])] = cpu->a;
			cpu->cycles += 6;
			
			break;
		}
		case 0x84: { // STY zpg
			cpu->memory[cpu->memory[cpu->pc++]] = cpu->y;
			cpu->cycles += 3;
			
			break;
		}
		case 0x85: { // STA zpg
			cpu->memory[cpu->memory[cpu->pc++]] = cpu->a;
			cpu->cycles += 3;
			
			break;
		}
		case 0x86: { // STX zpg
			cpu->memory[cpu->memory[cpu->pc++]] = cpu->x;
			cpu->cycles += 3;
			
			break;
		}
		case 0x88: { // DEY impl
			cpu->y -= 0x1;
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0x8A: { // TXA impl
			cpu->a = cpu->x;
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0x8C: { // STY abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);			
			cpu->memory[mem_location] = cpu->y;
			cpu->cycles += 4;
			
			break;
		}
		case 0x8D: { // STA abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);			
			cpu->memory[mem_location] = cpu->a;
			cpu->cycles += 4;
			
			break;
		}
		case 0x8E: { // STX abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);			
			cpu->memory[mem_location] = cpu->x;
			cpu->cycles += 4;
			
			break;
		}
		case 0x90: { // BCC rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x1) == 0)); // if bit 0 is off
			cpu->cycles += 2;
			
			break;
		}
		case 0x91: { // STA ind,Y
			cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], NULL)] = cpu->a;
			cpu->cycles += 6;
			
			break;
		}
		case 0x94: { // STY zpg,X
			cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])] = cpu->y;
			cpu->cycles += 4;
			
			break;
		}
		case 0x95: { // STA zpg,X
			cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])] = cpu->a;
			cpu->cycles += 4;
			
			break;
		}
		case 0x96: { // STX zpg,Y
			cpu->memory[addressForZeroPageYAddressing(cpu, cpu->memory[cpu->pc++])] = cpu->x;
			cpu->cycles += 4;
			
			break;
		}
		case 0x98: { // TYA impl
			cpu->a = cpu->y;
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0x99:   // STA abs,Y
		case 0x9D: { // STA abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			
			cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0x99 ? cpu->y : cpu->x), NULL)] = cpu->a;
			cpu->cycles += 5;
			
			break;
		}
		case 0x9A: { // TXS impl
			cpu->sp = cpu->x;
			updateStatusRegister(cpu, cpu->sp, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xA0: { // LDY immediate
			cpu->y = cpu->memory[cpu->pc++];
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xA1: { // LDA ind,X
			cpu->a = cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 6;
			
			break;
		}
		case 0xA2: { // LDX immediate
			cpu->x = cpu->memory[cpu->pc++];
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xA4: { // LDY zpg
			cpu->y = cpu->memory[cpu->memory[cpu->pc++]];
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 3;
			
			break;
		}
		case 0xA5: { // LDA zpg
			cpu->a = cpu->memory[cpu->memory[cpu->pc++]];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 3;
			
			break;
		}
		case 0xA6: { // LDX zpg
			cpu->x = cpu->memory[cpu->memory[cpu->pc++]];
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 3;
			
			break;
		}
		case 0xA8: { // TAY impl
			cpu->y = cpu->a;
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xA9: { // LDA immediate
			cpu->a = cpu->memory[cpu->pc++];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xAA: { // TAX impl
			cpu->x = cpu->a;
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xAC: { // LDY abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			cpu->y = cpu->memory[mem_location];
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xAD: { // LDA abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			cpu->a = cpu->memory[mem_location];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xAE: { // LDX abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			cpu->x = cpu->memory[mem_location];
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xB0: { // BCS rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x1) != 0)); // if bit 0 is on
			cpu->cycles += 2;
			
			break;
		}
		case 0xB1: { // LDA ind,Y
			cpu->a = cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 5;
			
			break;
		}
		case 0xB4: { // LDY zpg,X
			cpu->y = cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xB5: { // LDA zpg,X
			cpu->a = cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xB6: { // LDX zpg,Y
			cpu->x = cpu->memory[addressForZeroPageYAddressing(cpu, cpu->memory[cpu->pc++])];
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xB8: { // CLV impl
			cpu->ps &= 0xBF; // set overflow bit off (bit 6)
			cpu->cycles += 2;
			
			break;
		}
		case 0xB9: { // LDA abs,Y
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			cpu->a = cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->y, &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xBA: { // TSX impl
			cpu->x = cpu->sp;
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xBC: { // LDY abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			cpu->y = cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->x, &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xBD: { // LDA abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			cpu->a = cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->x, &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->a, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xBE: { // LDX abs,Y
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			cpu->x = cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->y, &(cpu->cycles))];
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 4;
			
			break;
		}
		case 0xC0: { // CPY immediate
			compareBytes(cpu, cpu->y, cpu->memory[cpu->pc++]);
			cpu->cycles += 2;
			
			break;
		}
		case 0xC1: { // CMP ind,X
			compareBytes(cpu, cpu->a, cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])]);
			cpu->cycles += 6;
			
			break;
		}
		case 0xC4: { // CPY zpg
			compareBytes(cpu, cpu->y, cpu->memory[cpu->memory[cpu->pc++]]);
			cpu->cycles += 3;
			
			break;
		}
		case 0xC5: { // CMP zpg
			compareBytes(cpu, cpu->a, cpu->memory[cpu->memory[cpu->pc++]]);
			cpu->cycles += 3;
			
			break;
		}
		case 0xC6: { // DEC zpg
			int mem_location = cpu->memory[cpu->pc++];
			cpu->memory[mem_location] -= 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 5;
			
			break;
		}
		case 0xC8: { // INY impl
			cpu->y += 0x1;
			updateStatusRegister(cpu, cpu->y, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xC9: { // CMP immediate
			compareBytes(cpu, cpu->a, cpu->memory[cpu->pc++]);
			cpu->cycles += 2;
			
			break;
		}
		case 0xCA: { // DEX impl
			cpu->x -= 0x1;
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xCC: { // CPY abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			compareBytes(cpu, cpu->y, cpu->memory[mem_location]);
			cpu->cycles += 4;
			
			break;
		}
		case 0xCD: { // CMP abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			compareBytes(cpu, cpu->a, cpu->memory[mem_location]);
			cpu->cycles += 4;
			
			break;
		}
		case 0xCE: { // DEC abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			cpu->memory[mem_location] -= 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 3;
			
			break;
		}
		case 0xD0: { // BNE rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x2) == 0)); // if bit 2 is off
			cpu->cycles += 2;
			
			break;
		}
		case 0xD1: { // CMP ind,Y
			compareBytes(cpu, cpu->a, cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], &(cpu->cycles))]);
			cpu->cycles += 5;
			
			break;
		}
		case 0xD5: { // CMP zpg,X
			compareBytes(cpu, cpu->a, cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])]);
			cpu->cycles += 4;
			
			break;
		}
		case 0xD6: { // DEC zpg,X
			int mem_location = addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++]);
			cpu->memory[mem_location] -= 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 6;
			
			break;
		}
		case 0xD8: { // CLD impl
			printf("Decimal mode is not supported on this emulator.\n");
			exit(-1);
		}
		case 0xD9:   // CMP abs,Y			
		case 0xDD: { // CMP abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			compareBytes(cpu, cpu->a, cpu->memory[(currentOpcode == 0xD9 ? addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->y, &(cpu->cycles)) : addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->x, &(cpu->cycles)))]);
			cpu->cycles += 4;
			
			break;
		}
		case 0xDE: { // DEC abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->x, NULL);
			cpu->memory[mem_location] -= 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 7;
			
			break;
		}
		case 0xE0: { // CPX immediate
			compareBytes(cpu, cpu->x, cpu->memory[cpu->pc++]);
			cpu->cycles += 2;
			
			break;
		}
		case 0xE1: { // SBC ind,X
			int operation_byte = cpu->memory[addressForIndexedIndirectAddressing(cpu, cpu->memory[cpu->pc++])];
			subtractWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 6;
			
			break;
		}
		case 0xE4: { // CPX zpg
			compareBytes(cpu, cpu->x, cpu->memory[cpu->memory[cpu->pc++]]);
			cpu->cycles += 3;
			
			break;
		}
		case 0xE5: { // SBC zpg
			int operation_byte = cpu->memory[cpu->memory[cpu->pc++]];
			subtractWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 3;
			
			break;
		}
		case 0xE6: { // INC zpg
			int mem_location = cpu->memory[cpu->pc++];
			cpu->memory[mem_location] += 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 5;
			
			break;
		}
		case 0xE8: { // INX impl
			cpu->x += 0x1;
			updateStatusRegister(cpu, cpu->x, 0);
			cpu->cycles += 2;
			
			break;
		}
		case 0xE9: { // SBC immediate
			int operation_byte = cpu->memory[cpu->pc++];
			subtractWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 2;
			
			break;
		}
		case 0xEA: { // NOP impl (does nothing)
			cpu->cycles += 2;
			
			break;
		}
		case 0xEC: { // CPX abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			compareBytes(cpu, cpu->x, cpu->memory[mem_location]);
			cpu->cycles += 4;
			
			break;
		}
		case 0xED: { // SBC abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);			
			int operation_byte = cpu->memory[mem_location];
			
			subtractWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 4;
			
			break;
		}
		case 0xEE: { // INC abs
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = joinBytes(low_byte, high_byte);
			cpu->memory[mem_location] += 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 6;
			
			break;
		}
		case 0xF0: { // BEQ rel
			int branch_address = cpu->memory[cpu->pc++];
			branchToRelativeAddressIf(cpu, branch_address, ((cpu->ps & 0x2) != 0)); // if bit 1 is on
			cpu->cycles += 2;
			
			break;
		}
		case 0xF1: { // SBC ind,Y
			int operation_byte = cpu->memory[addressForIndirectIndexedAddressing(cpu, cpu->memory[cpu->pc++], &(cpu->cycles))];
			subtractWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 5;
			
			break;
		}
		case 0xF5: { // SBC zpg,X
			int operation_byte = cpu->memory[addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++])];
			subtractWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 4;
			
			break;
		}
		case 0xF6: { // INC zpg,X
			int mem_location = addressForZeroPageXAddressing(cpu, cpu->memory[cpu->pc++]);
			cpu->memory[mem_location] += 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 6;
			
			break;
		}
		case 0xF8: { // SED impl
			printf("Decimal flag is not supported on this emulator.\n");
			exit(-1);
		}
		case 0xF9:   // SBC abs,Y
		case 0xFD: { // SBC abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int operation_byte = cpu->memory[addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, (currentOpcode == 0xF9 ? cpu->y : cpu->x), &cpu->cycles)];
			
			subtractWithCarry(cpu, operation_byte);
			updateStatusRegister(cpu, cpu->a, 0x1); // 0x1 = ignore carry bit when settings processor status flags
			cpu->cycles += 4;
			
			break;
		}
		case 0xFE: { // INC abs,X
			unsigned char low_byte = cpu->memory[cpu->pc++];
			unsigned char high_byte = cpu->memory[cpu->pc++];
			int mem_location = addressForAbsoluteAddedAddressing(cpu, low_byte, high_byte, cpu->x, NULL);
			cpu->memory[mem_location] += 0x1;
			updateStatusRegister(cpu, cpu->memory[mem_location], 0);
			cpu->cycles += 7;
			
			break;
		}
		default: {
			printf("Crash. Trying to run unknown opcode (%x).\n", currentOpcode);
			exit(-1);
			break;
		}
	}
}

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
	// cpu.pc += 710;

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
		// printMemory(&cpu);
		printf("\n\n\n");
		printf("cpu->sp: %i\n", cpu.sp);
		printf("cpu->a: %i\n", cpu.a);
		printf("cpu->x: %i\n", cpu.x);
		printf("cpu->y: %i\n", cpu.y);
		printf("cpu->ps: %i\n\n", cpu.ps);
		
		// if(cpu.pc == 0) {
		// 	break;
		// }
	}
	
	printMemory(&cpu);
	
	printf("### results:\n");
	printf("cpu->sp: %i\n", cpu.sp);
	printf("cpu->a: %i\n", cpu.a);
	printf("cpu->x: %i\n", cpu.x);
	printf("cpu->y: %i\n", cpu.y);
	printf("cpu->ps: %i\n", cpu.ps);
	printf("cpu->cycles: %i\n", cpu.cycles);
	// printf("%s\n", );
	// printbitssimple(cpu.ps);	
	printf("MEMORY: %x\n", cpu.memory[0x0040]);
	
	freeCPU(&cpu);

	return 0;
}