#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <io.h>
#include <fonts.h>
#include <dogeio.h>

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t limine_request_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t limine_request_end_marker[] = LIMINE_REQUESTS_END_MARKER;

typedef struct __attribute__((packed)) {
    uint64_t kernel_stack;
    uint64_t user_stack_scratch;
} cpu_gs_t;

static cpu_gs_t bootstrap_cpu_gs;

void init_cpu_gs(uint64_t kernel_stack_top) {
    bootstrap_cpu_gs.kernel_stack = kernel_stack_top;
    bootstrap_cpu_gs.user_stack_scratch = 0;

    uint64_t gs_base = (uint64_t)&bootstrap_cpu_gs;
    __asm__ volatile (
        "wrmsr"
        :
        : "c" (0xC0000102),
          "a" ((uint32_t)(gs_base & 0xFFFFFFFF)),
          "d" ((uint32_t)(gs_base >> 32))
    );
}

void windoge(void) {
    clear();
    printf("Welcome to WindogeOS\n");
    init_serial();
    printf("[\033[96m ... \033[0m] Init GDT and TSS\n");
    init_gdt_tss();
    printf("[\033[92m wow \033[0m] GDT loaded\n");
    printf("[\033[96m ... \033[0m] Init IDT\n");
    init_idt();
    printf("[\033[92m wow \033[0m] IDT loaded\n");
    printf("[\033[96m ... \033[0m] Init FPU\n");
    init_fpu();
    printf("[\033[92m wow \033[0m] FPU done\n");
    printf("[\033[96m ... \033[0m] Init APIC\n");
    init_apic();
    printf("[\033[92m wow \033[0m] APIC done\n");
    printf("[\033[96m ... \033[0m] Init PCI\n");
    init_apic();
    printf("[\033[92m wow \033[0m] PCI done\n");
    printf("[\033[96m ... \033[0m] Init CPU GS\n");
    
    uint64_t current_rsp;
    __asm__ volatile ("movq %%rsp, %0" : "=r"(current_rsp));
    init_cpu_gs(current_rsp);
    
    printf("[\033[92m wow \033[0m] Init CPU GS sucessful\n");

    printf("[\033[96m ... \033[0m] Init PCIE...\n");
    init_pcie();
    printf("[\033[92m wow \033[0m] PCIE done\n");
    printf("[\033[96m ... \033[0m] Init AHCI (and SATA)...\n");
    ata_probe_devices();
    printf("[\033[92m wow \033[0m] AHCI and SATA done\n");
    printf("[\033[96m ... \033[0m] Init USB\n");
    init_keyboard();
    printf("[\033[92m wow \033[0m] USB done\n");
    printf("[\033[96m ... \033[0m] Init RTC (time)\n");
    init_rtc_time();
    printf("[\033[92m wow \033[0m] RTC done\n");
    printf("[\033[96m ... \033[0m] Mounting ExFAT\n");
    if (fs_mount()) {
        printf("[\033[91m bad \033[0m] Mount failed\n");
        halt();
    }
    printf("[\033[92m wow \033[0m] Mount done\n");
    printf("\033[96mWindogeOS has booted. \033[0m\n");
    printf("\033[96mUh, now what?\033[0m");
    halt();
}