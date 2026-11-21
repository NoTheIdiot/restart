#ifndef SYSCALLS_H
#define SYSCALLS_H

enum {
    TEXT = 0x01,
    FS   = 0x02,
    DOGE = 0x03,
    MEM  = 0x04,
    IPC  = 0x05,
    DEV  = 0x06,
};

enum {
    TEXT_PRINTF = 0x01
};

#define ERR_UNKNOWN_SYSCALL ((uint64_t)-1)
#define ERR_UNKNOWN_OPCODE  ((uint64_t)-2)
#define ERR_BAD_PTR         ((uint64_t)-3)
#define ERR_INVAL           ((uint64_t)-4)

#endif