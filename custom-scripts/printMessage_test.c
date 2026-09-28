#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_PRINTMESSAGE 387

int main(void)
{
    const char *msg = "Ola kernel";

    printf("Enviando mensagem para o kernel...\n");

    syscall(SYSCALL_PRINTMESSAGE, msg);

    return 0;
}