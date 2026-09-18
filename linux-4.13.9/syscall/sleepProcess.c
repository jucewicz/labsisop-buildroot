#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "sleepProcess.h"

asmlinkage long sys_listSleepingProcesses(pid_t __user *buf, int size)
{
    struct task_struct *proces;
    pid_t kbuf[256];
    int count = 0;
    int ret;

    if (size > 256)
        size = 256;

    if (size <= 0)
        return -1;

    for_each_process(proces) {
        if (proces->state == TASK_INTERRUPTIBLE ||
            proces->state == TASK_UNINTERRUPTIBLE) {

            if (count >= size)
                break;

            kbuf[count] = task_pid_nr(proces);
            count++;
        }
    }

    ret = copy_to_user(
        buf,
        kbuf,
        count * sizeof(pid_t)
    );

    if (ret != 0)
        return -1;

    return count;
}