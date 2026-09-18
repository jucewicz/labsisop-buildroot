#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/types.h>

#define SYSCALL_SLEEPING_PROCESSES 386

int main(void)
{
    pid_t buf[256];
    long ret;
    int i;

    ret = syscall(
        SYSCALL_SLEEPING_PROCESSES,
        buf,
        256
    );

    if (ret >= 0) {
        printf("Processos em sleep: %ld\n", ret);

        for (i = 0; i < ret; i++) {
            printf("PID: %d\n", buf[i]);
        }
    }
    else {
        printf("Erro ao executar a syscall: %ld\n", ret);
    }

    return 0;
}