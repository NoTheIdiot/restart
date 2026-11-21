#ifndef SYSCALLS_H
#define SYSCALLS_H

enum {
    TEXT = 1,
    FS   = 2,
    DOGE = 3,
    MEM  = 4,
    IPC  = 5,
    DEV  = 6,
};

enum {
    TEXT_PRINTF = 0x01
};

#define ERR_UNKNOWN_SYSCALL ((uint64_t)-1)
#define ERR_UNKNOWN_OPCODE  ((uint64_t)-2)
#define ERR_BAD_PTR         ((uint64_t)-3)
#define ERR_INVAL           ((uint64_t)-4)

#endif