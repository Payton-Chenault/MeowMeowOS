all:
	nasm -f bin ./src/boot.asm -o ./bin/boot.bin

clean:
	rm -f ./bin/boot.bin

build:
	make clean
	make all
	qemu-system-x86_64 -hda ./bin/boot.bin