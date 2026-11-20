[bits 64]

; headers (also known as FUCKING GLOBALS AND EXTERN)
global syscall_entry
extern syscall_handler

section .text
syscall_entry:
    ; to this day, i will never know what this does
    swapgs

    ; save rsp into stack
    mov [gs:8], r15
    mov r15, rsp
    mov rsp, [gs:0]
    push r15
    mov r15, [gs:8]

    ; enable interrupts
    sti 

    ; push almost all regs
    push rcx
    push r11
    push r15
    push r14
    push r13
    push r12
    push r10                              
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rbx
    push rax 

    ; pass pointer to stack frame
    mov rdi, rsp
    call syscall_handler

    ; send value
    mov [rsp], rax

    ; pop in reverse because stacks are like stacks of pancakes
    pop rax                               
    pop rbx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r12
    pop r13
    pop r14
    pop r15
    
    ; restore the RFLAGS and RIP
    pop r11                               
    pop rcx 

    ; disable interrupts
    cli
    pop rsp
    swapgs

    ; exit ring 0
    o64 sysret                      
    