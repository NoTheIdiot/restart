#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <io.h>

extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;

#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_PWT      (1ULL << 3)
#define PTE_PCD      (1ULL << 4)
#define PTE_HUGE     (1ULL << 7)
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_MSR_ENABLE 0x800
#define IA32_APIC_BASE_MSR_X2APIC 0x400
#define IA32_APIC_BASE_ADDRESS_MASK 0x0000000FFFFFF000ULL

#define APIC_REGISTER_MASKED 0x10000U
#define APIC_SPURIOUS_VECTOR 0xFFU

#define PIC_MASTER_COMMAND_PORT 0x20
#define MASTER_PIC_DATA_PORT    0x21
#define PIC_SLAVE_COMMAND_PORT  0xA0
#define SLAVE_PIC_DATA_PORT     0xA1
#define CPUID_FEAT_EDX_APIC     (1U << 9)

#define APIC_REG_TPR             0x0080U
#define APIC_REG_EOI             0x00B0U
#define APIC_REG_SVR             0x00F0U
#define APIC_REG_LVT_TIMER       0x0320U
#define APIC_REG_LVT_THERMAL     0x0330U
#define APIC_REG_LVT_PERF        0x0340U
#define APIC_REG_LVT_LINT0       0x0350U
#define APIC_REG_LVT_LINT1       0x0360U
#define APIC_REG_LVT_ERR         0x0370U
#define APIC_SVR_SOFTWARE_ENABLE 0x100U

#define APIC_REG_TIMER_INITCNT 0x0380U
#define APIC_REG_TIMER_CURRCNT 0x0390U
#define APIC_REG_TIMER_DIV     0x03E0U
#define APIC_TIMER_PERIODIC    (1U << 17)

static uintptr_t lapic_base = 0;
volatile uint32_t ticks = 0;

static uint64_t alloc_page_early(void) {
    static size_t entry_idx = 0;
    static uint64_t offset = 0;

    struct limine_memmap_response *mmap = memmap_request.response;
    if (!mmap) return 0;

    while (entry_idx < mmap->entry_count) {
        struct limine_memmap_entry *e = mmap->entries[entry_idx];
        if (e->type == LIMINE_MEMMAP_USABLE && offset + 4096 <= e->length) {
            uint64_t phys = e->base + offset;
            offset += 4096;
            uint8_t *virt = (uint8_t *)(phys + hhdm_request.response->offset);
            for (size_t i = 0; i < 4096; i++) virt[i] = 0;
            return phys;
        }
        entry_idx++;
        offset = 0;
    }
    return 0;
}

static uint64_t *get_or_alloc_table(uint64_t *table, size_t idx) {
    uint64_t hhdm = hhdm_request.response->offset;
    if (!(table[idx] & PTE_PRESENT)) {
        uint64_t new_phys = alloc_page_early();
        if (!new_phys) return NULL;
        table[idx] = new_phys | PTE_PRESENT | PTE_WRITABLE;
    }
    return (uint64_t *)((table[idx] & PTE_ADDR_MASK) + hhdm);
}

void *map_kernel_mmio(uint64_t phys_addr, size_t size) {
    uint64_t hhdm = hhdm_request.response->offset;
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    uint64_t *pml4 = (uint64_t *)((cr3 & PTE_ADDR_MASK) + hhdm);
    uint64_t start = phys_addr & ~0xFFFULL;
    uint64_t end   = (phys_addr + size + 0xFFFULL) & ~0xFFFULL;

    for (uint64_t p = start; p < end; p += 4096) {
        uint64_t v = p + hhdm;
        size_t i4 = (v >> 39) & 0x1FF;
        size_t i3 = (v >> 30) & 0x1FF;
        size_t i2 = (v >> 21) & 0x1FF;
        size_t i1 = (v >> 12) & 0x1FF;

        uint64_t *pdpt = get_or_alloc_table(pml4, i4);
        if (!pdpt) return NULL;
        uint64_t *pd = get_or_alloc_table(pdpt, i3);
        if (!pd) return NULL;
        uint64_t *pt = get_or_alloc_table(pd, i2);
        if (!pt) return NULL;

        pt[i1] = p | PTE_PRESENT | PTE_WRITABLE | PTE_PWT | PTE_PCD;
        __asm__ volatile ("invlpg (%0)" :: "r"(v) : "memory");
    }

    return (void *)(phys_addr + hhdm);
}

void cpu_set_apic_base(uintptr_t apic) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);

    msr_val &= ~IA32_APIC_BASE_ADDRESS_MASK;
    msr_val |= ((uint64_t)apic & IA32_APIC_BASE_ADDRESS_MASK) |
               IA32_APIC_BASE_MSR_ENABLE;

    wrmsr(IA32_APIC_BASE_MSR, msr_val);
}

uintptr_t cpu_get_apic_base(void) {
    uint64_t msr_val = rdmsr(IA32_APIC_BASE_MSR);
    return (uintptr_t)(msr_val & IA32_APIC_BASE_ADDRESS_MASK);
}

static inline void write_reg(uint32_t offset, uint32_t val) {
    mmio_write32(lapic_base + offset, val);
}

bool check_apic(void) {
    uint32_t eax, edx;
    cpuid(1, &eax, &edx);
    return (edx & CPUID_FEAT_EDX_APIC) != 0;
}

static void io_wait(void) {
    (void)inb(0x80);
}

void disable_legacy_pic(void) {
    outb(MASTER_PIC_DATA_PORT, 0xFF);
    outb(SLAVE_PIC_DATA_PORT, 0xFF);

    outb(PIC_MASTER_COMMAND_PORT, 0x11);
    io_wait();
    outb(PIC_SLAVE_COMMAND_PORT, 0x11);
    io_wait();
    outb(MASTER_PIC_DATA_PORT, 0x20);
    io_wait();
    outb(SLAVE_PIC_DATA_PORT, 0x28);
    io_wait();
    outb(MASTER_PIC_DATA_PORT, 0x04);
    io_wait();
    outb(SLAVE_PIC_DATA_PORT, 0x02);
    io_wait();
    outb(MASTER_PIC_DATA_PORT, 0x01);
    io_wait();
    outb(SLAVE_PIC_DATA_PORT, 0x01);
    io_wait();

    outb(MASTER_PIC_DATA_PORT, 0xFF);
    outb(SLAVE_PIC_DATA_PORT, 0xFF);
}

void init_apic(void) {
    if (!check_apic() || hhdm_request.response == NULL) {
        return;
    }

    disable_legacy_pic();

    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    if (apic_base_msr & IA32_APIC_BASE_MSR_X2APIC) {
        return;
    }

    uintptr_t phys_base = cpu_get_apic_base();
    lapic_base = (uintptr_t)map_kernel_mmio((uint64_t)phys_base, 4096);

    if (lapic_base == 0) {
        return;
    }

    cpu_set_apic_base(phys_base);

    write_reg(APIC_REG_TPR, 0);

    write_reg(APIC_REG_LVT_TIMER, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_THERMAL, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_PERF, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_ERR, APIC_REGISTER_MASKED);

    write_reg(APIC_REG_LVT_LINT0, APIC_REGISTER_MASKED);
    write_reg(APIC_REG_LVT_LINT1, APIC_REGISTER_MASKED);

    write_reg(APIC_REG_SVR, APIC_SPURIOUS_VECTOR | APIC_SVR_SOFTWARE_ENABLE);
}

void apic_eoi(void) {
    if (lapic_base != 0) {
        write_reg(APIC_REG_EOI, 0);
    }
}

void init_apic_timer(uint8_t vector, uint32_t initial_count) {
    if (lapic_base == 0) return;

    write_reg(APIC_REG_TIMER_DIV, 0x3);
    write_reg(APIC_REG_LVT_TIMER, (uint32_t)vector | APIC_TIMER_PERIODIC);
    write_reg(APIC_REG_TIMER_INITCNT, initial_count);
}

void apic_tick(void) {
    ticks++;
    apic_eoi();
}

void sleep_ms(uint32_t ms) {
    uint32_t start = ticks;
    while ((uint32_t)(ticks - start) < ms) {
        __asm__ volatile ("hlt");
    }
}
