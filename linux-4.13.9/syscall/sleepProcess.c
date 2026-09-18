#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "sleepProcess.h"

asmlinkage long sys_listSleepingProcesses(char __user *buf, int size)
{
    struct task_struct *proces;
    char *kbuf;
    int offset = 0;
    int written;
    int ret;

    if (size <= 0)
        return -1;

    /*
     * Aloca um buffer dentro do kernel.
     * Esse buffer será usado para montar a lista de processos.
     */
    kbuf = kmalloc(size, GFP_KERNEL);

    if (kbuf == NULL)
        return -1;

    /*
     * Percorre todos os processos existentes no sistema.
     */
    for_each_process(proces) {

        /*
         * Verifica se o processo está em sleep.
         */
        if (proces->state == TASK_INTERRUPTIBLE ||
            proces->state == TASK_UNINTERRUPTIBLE) {

            /*
             * Adiciona as informações do processo
             * na próxima posição livre de kbuf.
             */
            written = scnprintf(
                kbuf + offset,
                size - offset,
                "Process: %s\n"
                "PID: %ld\n"
                "State: %ld\n\n",
                proces->comm,
                (long)task_pid_nr(proces),
                (long)proces->state
            );

            offset += written;

            /*
             * Buffer cheio.
             */
            if (offset >= size - 1)
                break;
        }
    }

    /*
     * Copia os dados do espaço do kernel
     * para o buffer da aplicação.
     */
    ret = copy_to_user(buf, kbuf, offset + 1);

    kfree(kbuf);

    if (ret != 0)
        return -1;

    return offset;
}