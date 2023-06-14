yasm -f bin bootsec.asm -o bootsec.bin
g++ -ffreestanding -m32 -o kernel.o -c kernel.cpp -fno-pie
ld --oformat binary -Ttext 0x10000 -o kernel.bin --entry=kmain -m elf_i386 kernel.o
