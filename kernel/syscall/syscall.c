#include <dogeio.h>
#include <string.h>
#include <stdint.h>
#include <syscalls.h>
#include <bool.h>
#include <io.h>

#define MSR_IA32_EFER            0xC0000080
#define MSR_IA32_STAR            0xC0000081
#define MSR_IA32_LSTAR           0xC0000082
#define MSR_IA32_FMASK           0xC0000084
#define MSR_IA32_KERNEL_GS_BASE  0xC0000102

#define EFER_SCE                 (1ULL << 0)
#define EFER_NXE                 (1ULL << 11)

#define RFLAGS_TF                (1ULL << 8)
#define RFLAGS_IF                (1ULL << 9)
#define RFLAGS_DF                (1ULL << 10)

#define SYS_EXIT                 60

#define USER_CODE_BASE           0x0000000000400000ULL
#define USER_STACK_BASE          0x00007FFFF0000000ULL
#define USER_ADDRESS_LIMIT       0x0000800000000000ULL
#define USER_LIMIT               0x0000800000000000ULL
#define USER_START               0x0000000010000000ULL

#define USER_IO_LIMIT            65536
#define USER_STRING_LIMIT        4096
#define USER_PATH_LIMIT          256
#define USER_INPUT_LIMIT         256
#define MAX_EXEC_ARGS 32
#define MAX_ARG_LEN   256

#ifndef TEXT_PRINTF
#define TEXT_PRINTF              0x01
#endif

#ifndef ERR_UNKNOWN_SYSCALL
#define ERR_UNKNOWN_SYSCALL      ((uint64_t)-1)
#endif

#ifndef ERR_UNKNOWN_OPCODE
#define ERR_UNKNOWN_OPCODE       ((uint64_t)-2)
#endif

#ifndef ERR_BAD_PTR
#define ERR_BAD_PTR              ((uint64_t)-3)
#endif

uint64_t heap_pointer = 0x0000000010000000ULL;

static bool is_valid_user_ptr(const void *ptr, size_t len) {
    uintptr_t addr = (uintptr_t)ptr;
    return addr != 0 && addr < USER_LIMIT && len <= (USER_LIMIT - addr);
}

static bool copy_str_from_user(char *dst, const char *user_src, size_t max_len) {
    if (!is_valid_user_ptr(user_src, 1) || max_len == 0) return false;
    for (size_t i = 0; i < max_len; i++) {
        if (!is_valid_user_ptr(&user_src[i], 1)) return false;
        dst[i] = user_src[i];
        if (dst[i] == '\0') return true;
    }
    dst[max_len - 1] = '\0';
    return false;
}

static bool copy_from_user(void *dst, const void *user_src, size_t len) {
    if (!is_valid_user_ptr(user_src, len)) return false;
    memcpy(dst, user_src, len);
    return true;
}

static bool copy_to_user(void *user_dst, const void *src, size_t len) {
    if (!is_valid_user_ptr(user_dst, len)) return false;
    memcpy(user_dst, src, len);
    return true;
}

extern void syscall_entry(void);

typedef struct {
    uint64_t kernel_rsp;
    uint64_t user_rsp_scratch;
} __attribute__((packed)) per_cpu_data_t;

static uint8_t syscall_stack[65536] __attribute__((aligned(16)));
static per_cpu_data_t bsp_cpu_data;
uint64_t kernel_program_launcher_rsp = 0;
uint64_t kernel_program_launcher_rflags = 0;
static bool cpu_supports_nx;
static bool cpu_supports_rdrand;

__attribute__((noreturn)) extern void syscall_exit_to_launcher(void);

static inline uint64_t handle_text(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case TEXT_PRINTF: {
            const char *user_str = (const char *)regs->rsi;
            size_t len = (size_t)regs->rdx;

            if (len == 0) return 0;
            if (len > USER_IO_LIMIT) len = USER_IO_LIMIT;

            char kbuf[256];
            size_t offset = 0;

            while (offset < len) {
                size_t chunk = len - offset;
                if (chunk > sizeof(kbuf) - 1) {
                    chunk = sizeof(kbuf) - 1;
                }

                if (!copy_from_user(kbuf, user_str + offset, chunk)) {
                    return ERR_BAD_PTR;
                }

                kbuf[chunk] = '\0';
                printf("%s", kbuf);
                offset += chunk;
            }

            return offset;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

uint64_t syscall_handler(registers_t *regs) {
    switch (regs->rax) {
        case TEXT: {
            return handle_text(regs);
        }

        default: {
            printf("Unknown syscall attempted to execute: %llu\n", regs->rax);
            return ERR_UNKNOWN_SYSCALL;
        }
    }
}

void init_syscalls(void) {
    uint32_t max_basic_leaf;
    uint32_t cpuid_ebx;
    uint32_t cpuid_ecx;
    uint32_t cpuid_edx;
    __asm__ volatile (
        "cpuid"
        : "=a"(max_basic_leaf), "=b"(cpuid_ebx),
          "=c"(cpuid_ecx), "=d"(cpuid_edx)
        : "a"(0), "c"(0)
    );
    (void)cpuid_ebx;
    (void)cpuid_edx;
    if (max_basic_leaf >= 1) {
        uint32_t basic_features;
        __asm__ volatile (
            "cpuid"
            : "=a"(basic_features), "=b"(cpuid_ebx),
              "=c"(cpuid_ecx), "=d"(cpuid_edx)
            : "a"(1), "c"(0)
        );
        (void)basic_features;
        cpu_supports_rdrand = (cpuid_ecx & (1U << 30)) != 0;
    }

    uint32_t max_extended_leaf;
    __asm__ volatile (
        "cpuid"
        : "=a"(max_extended_leaf), "=b"(cpuid_ebx),
          "=c"(cpuid_ecx), "=d"(cpuid_edx)
        : "a"(0x80000000U), "c"(0)
    );
    (void)cpuid_ebx;
    (void)cpuid_ecx;
    if (max_extended_leaf >= 0x80000001U) {
        uint32_t extended_features;
        __asm__ volatile (
            "cpuid"
            : "=a"(extended_features), "=b"(cpuid_ebx),
              "=c"(cpuid_ecx), "=d"(cpuid_edx)
            : "a"(0x80000001U), "c"(0)
        );
        (void)extended_features;
        (void)cpuid_ebx;
        (void)cpuid_ecx;
        cpu_supports_nx = (cpuid_edx & (1U << 20)) != 0;
    }
    if (cpu_supports_nx) {
        wrmsr(MSR_IA32_EFER, rdmsr(MSR_IA32_EFER) | EFER_NXE);
    }

    wrmsr(MSR_IA32_EFER, rdmsr(MSR_IA32_EFER) | EFER_SCE);

    uint64_t kernel_cs = 0x08; 
    uint64_t user_base = 0x10;
    
    uint64_t star = (kernel_cs << 32) | (user_base << 48);
    wrmsr(MSR_IA32_STAR, star);

    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_IA32_FMASK, RFLAGS_IF | RFLAGS_TF | RFLAGS_DF);

    bsp_cpu_data.kernel_rsp = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    bsp_cpu_data.user_rsp_scratch = 0;

    wrmsr(MSR_IA32_KERNEL_GS_BASE, (uint64_t)&bsp_cpu_data);
}
