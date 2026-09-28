/*
 * SSTF IO Scheduler
 *
 * For Kernel 4.13.9
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

/* SSTF data structure. */
struct sstf_data {
    struct list_head queue;
    sector_t current_sector;
};

static void sstf_merged_requests(struct request_queue *q,
                                 struct request *rq,
                                 struct request *next)
{
    list_del_init(&next->queuelist);
}

/*
 * Esta função despacha o próximo bloco a ser lido.
 */
static int sstf_dispatch(struct request_queue *q, int force)
{
    struct sstf_data *nd = q->elevator->elevator_data;

    char direction = 'R';

    struct request *rq;
    struct request *iter;
    struct request *best = NULL;

    sector_t sector;
    sector_t distance;
    sector_t best_distance = (sector_t)-1;

    /*
     * Percorre todas as requisições atualmente presentes
     * na fila e procura aquela que está mais próxima
     * do setor atual.
     */
    list_for_each_entry(iter, &nd->queue, queuelist) {

        sector = blk_rq_pos(iter);

        /*
         * Calcula a distância absoluta entre:
         *
         * setor da requisição
         * e
         * setor atual do disco
         */
        if (sector >= nd->current_sector)
            distance = sector - nd->current_sector;
        else
            distance = nd->current_sector - sector;

        /*
         * Se encontramos uma requisição mais próxima,
         * ela passa a ser a melhor candidata.
         */
        if (distance < best_distance) {
            best_distance = distance;
            best = iter;
        }
    }

    /*
     * A melhor requisição encontrada será despachada.
     */
    rq = best;

    if (rq) {

        /*
         * Remove a requisição da fila do SSTF.
         */
        list_del_init(&rq->queuelist);

        /*
         * Envia a requisição para processamento.
         */
        elv_dispatch_sort(q, rq);

        /*
         * Depois de atender essa requisição,
         * o setor atual passa a ser o setor dela.
         */
        nd->current_sector = blk_rq_pos(rq);

        /*
         * Mostra a ordem em que as requisições
         * são atendidas.
         */
        printk(KERN_EMERG "[SSTF] dsp %c %llu\n",
               direction,
               (unsigned long long)blk_rq_pos(rq));

        return 1;
    }

    return 0;
}

/*
 * Adiciona uma nova requisição na fila do scheduler.
 */
static void sstf_add_request(struct request_queue *q,
                             struct request *rq)
{
    struct sstf_data *nd = q->elevator->elevator_data;

    char direction = 'R';

    /*
     * Adiciona a nova requisição no final da lista.
     *
     * A ordem física da lista continua sendo a
     * ordem de chegada.
     *
     * Quem escolhe qual será atendida é
     * sstf_dispatch().
     */
    list_add_tail(&rq->queuelist, &nd->queue);

    /*
     * Mostra a ordem de chegada das requisições.
     */
    printk(KERN_EMERG "[SSTF] add %c %llu\n",
           direction,
           (unsigned long long)blk_rq_pos(rq));
}

/*
 * Inicialização da fila do SSTF.
 */
static int sstf_init_queue(struct request_queue *q,
                           struct elevator_type *e)
{
    struct sstf_data *nd;
    struct elevator_queue *eq;

    /*
     * Cria a estrutura genérica do elevator.
     */
    eq = elevator_alloc(q, e);

    if (!eq)
        return -ENOMEM;

    /*
     * Aloca os dados privados do nosso SSTF.
     */
    nd = kmalloc_node(sizeof(*nd),
                      GFP_KERNEL,
                      q->node);

    if (!nd) {
        kobject_put(&eq->kobj);
        return -ENOMEM;
    }

    /*
     * Liga os dados privados do SSTF
     * à estrutura genérica do elevator.
     */
    eq->elevator_data = nd;

    /*
     * Inicializa a lista de requisições.
     */
    INIT_LIST_HEAD(&nd->queue);

    /*
     * Consideramos inicialmente que a cabeça
     * do disco está no setor 0.
     */
    nd->current_sector = 0;

    /*
     * Associa o scheduler à request_queue.
     */
    spin_lock_irq(q->queue_lock);

    q->elevator = eq;

    spin_unlock_irq(q->queue_lock);

    return 0;
}

/*
 * Finalização da fila.
 */
static void sstf_exit_queue(struct elevator_queue *e)
{
    struct sstf_data *nd = e->elevator_data;

    /*
     * A fila deve estar vazia quando
     * o scheduler for removido.
     */
    BUG_ON(!list_empty(&nd->queue));

    /*
     * Libera a memória dos dados privados.
     */
    kfree(nd);
}

/*
 * Infraestrutura dos drivers de IO Scheduling.
 */
static struct elevator_type elevator_sstf = {
    .ops.sq = {
        .elevator_merge_req_fn = sstf_merged_requests,
        .elevator_dispatch_fn  = sstf_dispatch,
        .elevator_add_req_fn   = sstf_add_request,
        .elevator_init_fn      = sstf_init_queue,
        .elevator_exit_fn      = sstf_exit_queue,
    },

    .elevator_name = "sstf",
    .elevator_owner = THIS_MODULE,
};

/*
 * Inicialização do módulo.
 */
static int __init sstf_init(void)
{
    return elv_register(&elevator_sstf);
}

/*
 * Finalização do módulo.
 */
static void __exit sstf_exit(void)
{
    elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);

MODULE_AUTHOR("Miguel Xavier");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");