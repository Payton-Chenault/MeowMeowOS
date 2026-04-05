FILES = ./build/kernel.asm.o ./build/kernel.o ./build/vga.o
FLAGS = -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc

all:
	nasm -f bin ./src/bootloader/boot.asm -o ./bin/boot.bin
	nasm -f elf -g ./src/kernel/kernel.asm -o ./build/kernel.asm.o

    # COMPILE ALL C FILES HERE
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel/kernel.c -o ./build/kernel.o
	i686-elf-gcc -I./src $(FLAGS) -std=gnu99 -c ./src/kernel/intf/vga_display/vga.c -o ./build/vga.o


	i686-elf-ld -g -relocatable $(FILES) -o ./build/completeKernel.o
	i686-elf-gcc $(FLAGS) -T ./src/linker.ld -o ./bin/kernel.bin -ffreestanding -O0 -nostdlib ./build/completeKernel.o

	rm -f ./bin/MeowMeowOS.bin

	dd if=./bin/boot.bin >> ./bin/MeowMeowOS.bin
	dd if=./bin/kernel.bin >> ./bin/MeowMeowOS.bin
	dd if=/dev/zero bs=512 count=32 >> ./bin/MeowMeowOS.bin
clean:
	rm -f ./bin/boot.bin
	rm -f ./bin/kernel.bin
	rm -f ./bin/MeowMeowOS.bin
	rm -f ./build/kernel.asm.o
	rm -f ./build/kernel.o
	rm -f ./build/vga.o
	rm -f ./build/completeKernel.o
run:
	make clean
	make all
	qemu-system-x86_64 -hda ./bin/MeowMeowOS.bin