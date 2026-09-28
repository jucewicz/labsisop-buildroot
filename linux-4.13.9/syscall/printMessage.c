#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

#include "printMessage.h"

asmlinkage long sys_printMessage(const char __user *msg)
{
    char kbuf[256];
    long ret;

    ret = strncpy_from_user(kbuf, msg, sizeof(kbuf) - 1);

    if (ret < 0)
        return ret;

    kbuf[sizeof(kbuf) - 1] = '\0';

    printk(KERN_INFO "[SYSCALL MESSAGE] %s\n", kbuf);

    return 0;
}