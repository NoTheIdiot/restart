[bits 64]

; show the other files that these functions actually exist
global to_userland_ring3
global to_userland_ring3_args
global syscall_exit_to_launcher

; get a kernel stack for rsp and rflags
extern kernel_program_launcher_rsp
extern kernel_program_launcher_rflags

section .text
; this one for basic ring3
to_userland_ring3:
    xor edx, edx
    xor ecx, ecx

; this one is actually useds
to_userland_ring3_args:
    mov r8, rdi
    mov r9, rsi
    mov r10, rdx
    mov r11, rcx

    cli

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    push qword [rel kernel_program_launcher_rsp]
    push qword [rel kernel_program_launcher_rflags]

    mov [rel kernel_program_launcher_rsp], rsp
    pushfq
    pop rax
    mov [rel kernel_program_launcher_rflags], rax

    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax

    swapgs

    push qword 0x1b
    push r9
    push qword 0x202
    push qword 0x23
    push r8

    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor edi, edi
    xor esi, esi
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d

    iretq

; exit back to where you came from
syscall_exit_to_launcher:
    cli

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    mov rsp, [rel kernel_program_launcher_rsp]

    mov rcx, [rel kernel_program_launcher_rflags]

    pop qword [rel kernel_program_launcher_rflags]
    pop qword [rel kernel_program_launcher_rsp]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    push rcx
    popfq

    xor eax, eax
    ret
