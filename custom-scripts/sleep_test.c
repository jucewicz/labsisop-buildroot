#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_SLEEPING_PROCESSES 386

int main(void)
{
    char buf[4096];
    long ret;

    printf("Invoking 'listSleepingProcesses' system call.\n");

    ret = syscall(
        SYSCALL_SLEEPING_PROCESSES,
        buf,
        sizeof(buf)
    );

    if (ret >= 0) {
        printf("Sleeping processes:\n\n");
        printf("%s\n", buf);
    }
    else {
        printf(
            "System call 'listSleepingProcesses' failed: %ld\n",
            ret
        );
    }

    return 0;
}