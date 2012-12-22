FILES += cpu.c
FILES += main.c
EXECUTABLE = 6502_emu

all:
	gcc $(FILES) $(LIBRARIES) -o $(EXECUTABLE)

run: all
	./$(EXECUTABLE) test.bin
