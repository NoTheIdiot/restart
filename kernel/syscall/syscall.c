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
    if (addr == 0 || addr >= USER_LIMIT) return false;
    if (len > USER_LIMIT - addr) return false;
    return true;
}

static bool copy_str_from_user(char *dst, const char *user_src, size_t max_len) {
    if (!dst || !user_src || max_len == 0) return false;
    for (size_t i = 0; i < max_len; i++) {
        if (!is_valid_user_ptr(&user_src[i], 1)) {
            dst[0] = '\0';
            return false;
        }
        char c = user_src[i];
        dst[i] = c;
        if (c == '\0') return true;
    }
    dst[max_len - 1] = '\0';
    return false;
}

static bool copy_from_user(void *dst, const void *user_src, size_t len) {
    if (len == 0) return true;
    if (!dst || !user_src || !is_valid_user_ptr(user_src, len)) return false;
    memcpy(dst, user_src, len);
    return true;
}

static bool copy_to_user(void *user_dst, const void *src, size_t len) {
    if (len == 0) return true;
    if (!user_dst || !src || !is_valid_user_ptr(user_dst, len)) return false;
    memcpy(user_dst, src, len);
    return true;
}

extern void syscall_entry(void);

typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t r11;
    uint64_t rcx;
} __attribute__((packed)) registers_t;

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
            if (!is_valid_user_ptr(user_str, 1)) {
                return ERR_BAD_PTR;
            }

            // Support both explicit length in RDX (if small/valid) and NUL-terminated strings
            size_t explicit_len = (size_t)regs->rdx;
            bool use_explicit = (explicit_len > 0 && explicit_len <= USER_IO_LIMIT);
            size_t max_bytes = use_explicit ? explicit_len : USER_IO_LIMIT;

            char kbuf[256];
            size_t total_printed = 0;
            size_t offset = 0;

            while (offset < max_bytes) {
                size_t chunk = max_bytes - offset;
                if (chunk > sizeof(kbuf) - 1) {
                    chunk = sizeof(kbuf) - 1;
                }

                size_t i = 0;
                bool hit_null = false;

                for (; i < chunk; i++) {
                    char ch;
                    if (!copy_from_user(&ch, user_str + offset + i, 1)) {
                        return ERR_BAD_PTR;
                    }
                    if (!use_explicit && ch == '\0') {
                        hit_null = true;
                        break;
                    }
                    if (use_explicit && ch == '\0') {
                        hit_null = true;
                        break;
                    }
                    kbuf[i] = ch;
                }

                kbuf[i] = '\0';

                if (i > 0) {
                    printf("%s", kbuf);
                    total_printed += i;
                }

                if (hit_null) {
                    break;
                }

                offset += i;
            }

            return total_printed;
        }

        default: {
            return ERR_UNKNOWN_OPCODE;
        }
    }
}

static inline uint64_t handle_doge(registers_t *regs) {
    uint64_t op = regs->rdi;

    switch (op) {
        case DOGE_EXIT: {
            sprintf("[SYSCALl] exiting");
            cleanup_pages();
            heap_pointer = USER_START;
            if (kernel_program_launcher_rsp == 0) {
                sprintf("[SYSCALL] ERROR: kernel_program_launcher_rsp is NULL again...");
                halt();
            }
            syscall_exit_to_launcher();
            return 0;
        }
    }
}

uint64_t syscall_handler(registers_t *regs) {
    sprintf("[SYSCALL] rax=%llu rdi=%llu rsi=%llu\n", regs->rax, regs->rdi, regs->rsi);
    switch (regs->rax) {
        case TEXT: {
            return handle_text(regs);
        }

        case DOGE: {
            return handle_doge(regs);
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