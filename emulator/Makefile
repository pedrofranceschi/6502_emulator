FILES += cpu.c
FILES += main.c
EXECUTABLE = 6502_emulator

all:
	gcc $(FILES) $(LIBRARIES) -o $(EXECUTABLE)

run: all
	./$(EXECUTABLE) test.bin
