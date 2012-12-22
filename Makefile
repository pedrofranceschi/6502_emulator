FILES += cpu.c
FILES += test.c
EXECUTABLE = 6502_emulator

all:
	gcc $(FILES) $(LIBRARIES) -o $(EXECUTABLE)

run: all
	./$(EXECUTABLE) test.bin
