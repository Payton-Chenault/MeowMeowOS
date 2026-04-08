#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stdbool.h>

#define ELF_MAGIC 0x464C457F
#define PT_LOAD   1


typedef struct {
    uint8_t  e_ident[16]; // 0x00: Magic number and architecture info
    uint16_t e_type;      // 0x10: 2 = Executable
    uint16_t e_machine;   // 0x12: 3 = x86 (i386)
    uint32_t e_version;   // 0x14: 1 = Original version
    uint32_t e_entry;     // 0x18: The memory address to jump to
    uint32_t e_phoff;     // 0x1C: Byte offset in the file where Program Headers start
    uint32_t e_shoff;     // 0x20: Byte offset in the file where Section Headers start
    uint32_t e_flags;     // 0x24: Architecture-specific flags
    uint16_t e_ehsize;    // 0x28: Size of this main header (usually 52 bytes)
    uint16_t e_phentsize; // 0x2A: Size of one Program Header (usually 32 bytes)
    uint16_t e_phnum;     // 0x2C: Total number of Program Headers
    uint16_t e_shentsize; // 0x2E: Size of one Section Header
    uint16_t e_shnum;     // 0x30: Total number of Section Headers
    uint16_t e_shstrndx;  // 0x32: Index of the section header string table
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    uint32_t p_type;      // 0x00: Segment type
    uint32_t p_offset;    // 0x04: Where in the file this segment's data starts
    uint32_t p_vaddr;     // 0x08: The Virtual Address where this belongs in RAM
    uint32_t p_paddr;     // 0x0C: Physical address
    uint32_t p_filesz;    // 0x10: How many bytes to copy from the file
    uint32_t p_memsz;     // 0x14: How much memory to allocate in total
    uint32_t p_flags;     // 0x18: Read(4) / Write(2) / Execute(1) permissions
    uint32_t p_align;     // 0x1C: Alignment requirement in memory
} __attribute__((packed)) elf32_phdr_t;

uint32_t elf_load_and_spawn(const char* filename);

#endif