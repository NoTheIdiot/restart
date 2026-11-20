#include <stdint.h>
#include <stddef.h>
#include <dogeio.h>
#include <bool.h>
#include <io.h>

const char *elf_error_messages[] = {
    "ELF_OK",
    "ELF_INVALID_MAGIC",
    "ELF_UNSUPPORTED_CLASS",
    "ELF_UNSUPPORTED_DATA_ENCODING",
    "ELF_UNSUPPORTED_VERSION",
    "ELF_UNSUPPORTED_TYPE",
    "ELF_UNSUPPORTED_MACHINE",
    "ELF_INVALID_PROGRAM_HEADER_OFFSET",
    "ELF_INVALID_SECTION_HEADER_OFFSET",
    "ELF_INVALID_PROGRAM_HEADER_SIZE",
    "ELF_INVALID_SECTION_HEADER_SIZE",
    "ELF_INVALID_PROGRAM_HEADER_COUNT",
    "ELF_INVALID_SECTION_HEADER_COUNT",
    "ELF_INVALID_SECTION_NAME_STRING_TABLE_INDEX",
    "ELF_UNSUPPORTED_OSABI"
};

typedef enum {
    ELF_OK = 0,
    ELF_INVALID_MAGIC,
    ELF_UNSUPPORTED_CLASS,
    ELF_UNSUPPORTED_DATA_ENCODING,
    ELF_UNSUPPORTED_VERSION,
    ELF_UNSUPPORTED_TYPE,
    ELF_UNSUPPORTED_MACHINE,
    ELF_INVALID_PROGRAM_HEADER_OFFSET,
    ELF_INVALID_SECTION_HEADER_OFFSET,
    ELF_INVALID_PROGRAM_HEADER_SIZE,
    ELF_INVALID_SECTION_HEADER_SIZE,
    ELF_INVALID_PROGRAM_HEADER_COUNT,
    ELF_INVALID_SECTION_HEADER_COUNT,
    ELF_INVALID_SECTION_NAME_STRING_TABLE_INDEX,
    ELF_UNSUPPORTED_OSABI
} status;

static const uint8_t magic[4] = { 0x7F, 'E', 'L', 'F' };
#define CLASS64 2
#define WINDOGE_MARK 0x44 // literally just 'D' for doge, for some reason
                          // i considered 0x55 for the FIVE FIVE

#define ELFDATA2LSB 1     // Little-endian (x86_64, ARM64, RISC-V)
#define EV_CURRENT  1     // Current ELF version

// what is this like
#define ET_EXEC     2     // static executable
#define ET_DYN      3     // shared object / PIE executable

// target arch
#define EM_X86_64   0x3E  // AMD x86-64
#define EM_AARCH64  0xB7  // ARM 64-bit
#define EM_RISCV    0xF3  // RISC-V

// header types
#define PT_NULL     0     // unused entry
#define PT_LOAD     1     // loadable segment (map into memory)
#define PT_DYNAMIC  2     // dynamic linking info
#define PT_INTERP   3     // path to dynamic linker
#define PT_NOTE     4     // auxiliary info
#define PT_PHDR     6     // program header table itself

// segment permissions
#define PF_X        0x1   // you can run this
#define PF_W        0x2   // you can write this
#define PF_R        0x4   // you only can read and not do shit

// oh look another header for the GOD DAMN HEADER PILE
typedef struct {
    uint8_t  e_ident[16];  // magic bytes & metadata
    uint16_t e_type;       // object file type (ET_EXEC, etc.)
    uint16_t e_machine;    // target ISA (EM_X86_64, etc.)
    uint32_t e_version;    // ELF version (EV_CURRENT)
    uint64_t e_entry;      // virtual address of entry point (_start)
    uint64_t e_phoff;      // file offset to Program Header table
    uint64_t e_shoff;      // file offset to Section Header table
    uint32_t e_flags;      // architecture-specific flags
    uint16_t e_ehsize;     // size of this header (64 bytes)
    uint16_t e_phentsize;  // size of one Program Header (56 bytes)
    uint16_t e_phnum;      // number of Program Headers
    uint16_t e_shentsize;  // size of one Section Header (64 bytes)
    uint16_t e_shnum;      // number of Section Headers
    uint16_t e_shstrndx;   // section header string table index
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;       // segment type (PT_LOAD, etc.)
    uint32_t p_flags;      // permissions (PF_R, PF_W, PF_X)
    uint64_t p_offset;     // offset of segment data in file
    uint64_t p_vaddr;      // target virtual address in userland
    uint64_t p_paddr;      // physical address (ignored for user apps)
    uint64_t p_filesz;     // bytes to copy from file
    uint64_t p_memsz;      // total bytes in memory (p_memsz - p_filesz = .bss)
    uint64_t p_align;      // segment alignment in memory/file
} __attribute__((packed)) Elf64_Phdr;

// validate if it's even good enough for running
status validate(const uint8_t* file_buf, size_t file_size, uint8_t* out) {
    if (!file_buf || file_size < sizeof(Elf64_Ehdr)) {
        return ELF_INVALID_MAGIC;
    }

    const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)file_buf;

    if (ehdr->e_ident[0] != magic[0] ||
        ehdr->e_ident[1] != magic[1] ||
        ehdr->e_ident[2] != magic[2] ||
        ehdr->e_ident[3] != magic[3]) {
        return ELF_INVALID_MAGIC;
    }

    if (ehdr->e_ident[4] != CLASS64)     return ELF_UNSUPPORTED_CLASS;
    if (ehdr->e_ident[5] != ELFDATA2LSB) return ELF_UNSUPPORTED_DATA_ENCODING;
    if (ehdr->e_ident[6] != EV_CURRENT)  return ELF_UNSUPPORTED_VERSION;

    uint8_t osabi = ehdr->e_ident[7];
    if (osabi != WINDOGE_MARK && osabi != 0x00 && osabi != 0x03) {
        return ELF_UNSUPPORTED_OSABI;
    }
    if (out) {
        *out = osabi;
    }

    if (ehdr->e_type != ET_EXEC)      return ELF_UNSUPPORTED_TYPE;
    if (ehdr->e_machine != EM_X86_64) return ELF_UNSUPPORTED_MACHINE;

    if (ehdr->e_phoff == 0 || ehdr->e_phoff >= file_size) {
        return ELF_INVALID_PROGRAM_HEADER_OFFSET;
    }
    if (ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        return ELF_INVALID_PROGRAM_HEADER_SIZE;
    }
    if (ehdr->e_phnum == 0 || (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * sizeof(Elf64_Phdr)) > file_size) {
        return ELF_INVALID_PROGRAM_HEADER_COUNT;
    }

    if (ehdr->e_entry == 0 || ehdr->e_entry >= 0x0000800000000000ULL) {
        return ELF_UNSUPPORTED_TYPE;
    }

    return ELF_OK;
}
