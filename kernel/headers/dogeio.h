#ifndef DOGEIO_H
#define DOGEIO_H

#include <stdint.h>
#include <bool.h>
#include <stddef.h>

#define TERMINAL_COLS 160
#define TERMINAL_ROWS 67

#define COLOR_BACKGROUND            0x0C0C0C
#define COLOR_DARK_GRAY             0x404040
#define COLOR_GRAY                  0xCCCCCC
#define COLOR_BRIGHT_GRAY           0xE0E0E0
#define COLOR_CURSOR                0xFFFFFF

#define COLOR_DARK_BLACK            0x000000
#define COLOR_DARK_BLUE             0x001F82
#define COLOR_DARK_GREEN            0x0B5E08
#define COLOR_DARK_CYAN             0x21547A
#define COLOR_DARK_RED              0x6F0912
#define COLOR_DARK_PURPLE           0x4D0D56
#define COLOR_DARK_MAGENTA          0x4D0D56
#define COLOR_DARK_ORANGE           0x803700
#define COLOR_DARK_YELLOW           0x6E5900
#define COLOR_DARK_WHITE            0x999999

#define COLOR_BLACK                 0x0C0C0C
#define COLOR_BLUE                  0x0037DA
#define COLOR_GREEN                 0x13A10E
#define COLOR_CYAN                  0x3A96DD
#define COLOR_RED                   0xC50F1F
#define COLOR_PURPLE                0x881798
#define COLOR_MAGENTA               0x881798
#define COLOR_ORANGE                0xD85D00
#define COLOR_YELLOW                0xC19C00
#define COLOR_WHITE                 0xF0F0F0

#define COLOR_BRIGHT_BLACK          0x767676
#define COLOR_BRIGHT_BLUE           0x3B78FF
#define COLOR_BRIGHT_GREEN          0x16C60C
#define COLOR_BRIGHT_CYAN           0x61D6D6
#define COLOR_BRIGHT_RED            0xE74856
#define COLOR_BRIGHT_PURPLE         0xB4009E
#define COLOR_BRIGHT_MAGENTA        0xB4009E
#define COLOR_BRIGHT_ORANGE         0xFF8C3A
#define COLOR_BRIGHT_YELLOW         0xF9F1A5
#define COLOR_BRIGHT_WHITE          0xFFFFFF

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

void clear(void);
void set_font(const uint8_t *font);
void putchar_at(char c, uint32_t x, uint32_t y);
void putchar(char c);
void vprintf(const char *fmt, va_list ap);
void printf(const char *fmt, ...);

// gdt stuff, or whatever. I do not want the 50 different header mess again
// or some garbage like that.
void init_gdt_tss(void);
void init_idt(void);
void init_fpu(void);
void init_apic(void);

#define ATA_DATA        0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DF   0x20
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80
#define ATA_TIMEOUT     1000000000U

#define ATA_DATA         0x1F0
#define ATA_FEATURES     0x1F1
#define ATA_SECTOR_CNT   0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE_HEAD   0x1F6
#define ATA_COMMAND      0x1F7
#define ATA_STATUS       0x1F7
#define ATA_DRIVE_COUNT  2

#ifndef BLOCK_SIZE
#define BLOCK_SIZE       512
#endif
#define ATA_PRIMARY_MASTER     0
#define ATA_PRIMARY_SLAVE      1
#define ATA_SECONDARY_MASTER   2
#define ATA_SECONDARY_SLAVE    3

typedef struct __attribute__((packed)) {
    uint8_t jump[3];        // 0xEB, 0x76, and 0x90
    char fs_name[8];        // is "EXFAT   "
    uint8_t zeros[53];      // /dev/zero that shi
    uint64_t offset;        // offset
    uint64_t volume;        // volume length
    uint32_t foffset;       // fat offset, from FAT origin
    uint32_t flength;       // fat length, size of a FAT in unit of 
                            // sector
    uint32_t choffset;      // cluster heap offset origin from the top 
                            // of the volume.
    uint32_t ccount;        // cluster count, about of clusters on drive.
    uint32_t fcord;         // first cluster at root
    uint32_t serial;        // volume serial number
    uint16_t fs_revision;   // file system revision
    uint16_t volume_flags;  // bit 0 is active FAT, bit 1 is volume dirty,
                            // bit 2 is failure, rest are zeros
    uint8_t sector_shift;   // shifting up
    uint8_t sectors_pch;    // sectirs per cluster shift
    uint8_t fat_num;        // number of fat
    uint8_t drive;          // yo what drive are we
    uint8_t percent_used;   // percnt of drive used, 0xFF is invalid (-1)
    uint8_t reserved[7];    // guess what, more zeros.
    uint8_t code[390];      // thats alot of empty space
    uint16_t sig;           // signature
} exfat_start_sector;

typedef struct __attribute__((packed)) {
    uint8_t entry_type; 
    uint8_t scount;
    uint16_t checksum;
    uint16_t attribute;
    uint16_t reserved1;
    uint32_t time_stamp;
    uint32_t last_modified;
    uint32_t last_accessed;
    uint8_t increment;
    uint8_t last_modified_increment;
    uint8_t TZoffset;
    uint8_t last_modified_TZoffset;
    uint8_t last_accessed_TZoffset;
    uint8_t reserved2[7];
} file_entry;

typedef struct __attribute__((packed)) {
    uint8_t entry_type;
    uint8_t secondary_flags;
    uint8_t reserved1;
    uint8_t name_length;
    uint16_t name_hash;
    uint16_t reserved2;
    uint64_t valid_data_length;
    uint32_t reserved3;
    uint32_t first_cluster;
    uint64_t data_length;
} stream_entry;

typedef struct __attribute__((packed)) {
    uint8_t entry_type;
    uint8_t secondary_flags;
    char filename[30];
} file_name_entry;

typedef struct {
    uint64_t poffset_bytes;

    uint32_t bytes_per_sector;      // guess what idiots, it's 512bytes.
    uint32_t sectors_per_cluster;   // how many sectors are in a single cluster
    uint32_t bytes_per_cluster;     // how about bytes are in a cluster?

    uint64_t offset_bytes;          // the offset from each fat (bad name microsoft)
    uint64_t heap_offset_bytes;
    uint32_t root_cluster;          // the cluster at root
    uint32_t cluster_count;         // mate, how many clusters are there?

    // second stage, where you get your hopes up then fall 
    // into depression, Also known has bitmaps and upcases.
    uint32_t bitmap_cluster;
    uint64_t bitmap_size;
    uint32_t upcase_cluster;
    uint64_t upcase_size;
} mount_t;

// idk what are bitmaps
typedef struct __attribute__((packed)) {
    uint8_t entry_type;
    uint8_t flags;
    uint8_t reserved[18];           // thought mainstream operating systems aren't bloated
                                    // enough? MORE RESERVED ZEROS :D
    uint32_t first_cluster;         // first cluster in a bitmap
    uint64_t data_length;           // size of bitmap in bytes
} bitmap;

// upcases
typedef struct __attribute__((packed)) {
    uint8_t entry_type;             // is 0x82
    uint8_t reserved1;              // guess what, more zeros.
    uint32_t checksum;              // checksum, idk the magic.
    uint8_t reserved[12];           // OH GOD MORE ZEROS HELP ME
    uint32_t first_cluster;         // first cluster of the upcase table
    uint64_t size;                  // size of table;
} upcase;

int mount(mount_t *vol, uint8_t *cluster_buf);
int create_file(mount_t *vol, const char *name);
int write_file(mount_t *vol, const char *name, const uint8_t* data, uint64_t size);
int read_file(mount_t *vol, const char *name, uint8_t* out_buf, uint64_t max_size, uint64_t *bytes_read_out);
int append_file(mount_t *vol, const char *name, const uint8_t* data, uint64_t size);
int create_dir(mount_t *vol, const char *path);
int delete_file(mount_t *vol, const char *name);
int change_dir(mount_t *vol, const char *path);
void get_cwd(char* buffer, int size);

typedef struct {
    char name[256];
    uint32_t size;
    bool is_dir;
} fs_t;

// paging macros
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PAGE_LARGE    (1ULL << 7)
#define PAGE_NO_EXECUTE (1ULL << 63)
#define PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define MSR_IA32_EFER 0xC0000080
#define EFER_NXE (1ULL << 11)
#define DATA_AMOUNT 8192
#define PAGE_SIZE 4096ULL
#define PAGE_USER_FLAGS (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)

void *map_kernel_mmio(uint64_t phys_addr, size_t size);
uint64_t alloc_page(void);

// pcie, sata, usb or some io shit
void pcie_init_system(uint64_t v_addr, uint8_t start_bus, uint8_t end_bus);
void init_pcie(void);
uint32_t pcie_read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset);
void pcie_write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg_offset, uint32_t value);
typedef struct {
    uint64_t address;
    uint64_t size;
    bool is_mmio;
    bool is_64bit;
} pci_bar_t;

void init_pci_ecam(uintptr_t rsdp_phys_addr);

uint32_t pci_read_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);
void     pci_write_32(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset, uint32_t val);
uint16_t pci_read_16(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);
uint8_t  pci_read_8(uint8_t bus, uint8_t slot, uint8_t func, uint16_t offset);

void      pci_enable_device(uint8_t bus, uint8_t slot, uint8_t func);
pci_bar_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index);
bool      pci_find_ahci(uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func, uint64_t *out_bar5_phys);

// sata and ahci
void ahci_sata_init(uintptr_t ecam_base_addr);
int ahci_exec_cmd(uint32_t port_num, uint64_t lba, uint32_t count, uint8_t write, uintptr_t buf_phys);

int ata_select_drive(uint8_t drive);
int ata_read_sector(uint8_t drive, uint64_t lba, uint8_t *buffer);
int ata_write_sector(uint8_t drive, uint64_t lba, const uint8_t *buffer);
void ata_probe_devices(void);

// usb (just xhci, wait usb 4 exists?)
int init_keyboard(void);
char get_key(void);

#endif
