/*
 * sched_profiler: mostra o padrao de escalonamento de N threads.
 *
 * Cada thread escreve a sua letra (A, B, C...) na proxima posicao livre de um
 * buffer global. No fim, o buffer mostra a ordem em que o escalonador deu a
 * CPU para cada thread.
 *
 * Uso:      sched_profiler <tamanho_buffer> <num_threads> <politica>
 * Politicas: SCHED_LOW_IDLE, SCHED_IDLE, SCHED_FIFO, SCHED_RR
 *            (SCHED_OTHER tambem e aceita, para comparacao)
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A glibc nao conhece a politica criada no kernel (include/uapi/linux/sched.h) */
#ifndef SCHED_LOW_IDLE
#define SCHED_LOW_IDLE	7
#endif

#define MAX_THREADS	26	/* uma letra por thread: A..Z */

static char *buffer;		/* buffer global */
static char *ptr;		/* ponteiro global: proxima posicao livre */
static char *buffer_end;	/* primeira posicao depois do fim do buffer */
static sem_t mutex;		/* semaforo binario: protege buffer e ptr */
static pthread_barrier_t start;	/* largada: as threads comecam juntas */

static const struct {
	const char *name;
	int policy;
} policies[] = {
	{ "SCHED_LOW_IDLE",	SCHED_LOW_IDLE },
	{ "SCHED_IDLE",		SCHED_IDLE },
	{ "SCHED_FIFO",		SCHED_FIFO },
	{ "SCHED_RR",		SCHED_RR },
	{ "SCHED_OTHER",	SCHED_OTHER },
};

static int parse_policy(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(policies) / sizeof(policies[0]); i++)
		if (strcmp(name, policies[i].name) == 0)
			return policies[i].policy;

	return -1;
}

static void *worker(void *arg)
{
	char letter = 'A' + (long)arg;

	/* Espera todas as threads estarem criadas e com a politica certa */
	pthread_barrier_wait(&start);

	for (;;) {
		sem_wait(&mutex);
		if (ptr >= buffer_end) {	/* buffer cheio: fim */
			sem_post(&mutex);
			break;
		}
		*ptr = letter;
		ptr++;
		sem_post(&mutex);
	}

	return NULL;
}

/*
 * Pos-processamento: junta letras iguais consecutivas (AAABBA -> ABA).
 * Cada bloco e uma vez em que a thread foi escalonada e escreveu.
 */
static void summarize(long size, int nthreads)
{
	int count[MAX_THREADS] = { 0 };
	char prev = 0;
	long i;

	for (i = 0; i < size; i++) {
		if (buffer[i] != prev) {
			putchar(buffer[i]);
			count[buffer[i] - 'A']++;
			prev = buffer[i];
		}
	}
	putchar('\n');

	for (i = 0; i < nthreads; i++)
		printf("%c = %d\n", (char)('A' + i), count[i]);
}

int main(int argc, char **argv)
{
	pthread_t threads[MAX_THREADS];
	struct sched_param param;
	long size, i;
	int nthreads, policy, ret;

	if (argc != 4) {
		fprintf(stderr, "uso: %s <tamanho_buffer> <num_threads> <politica>\n", argv[0]);
		fprintf(stderr, "politicas: SCHED_LOW_IDLE, SCHED_IDLE, SCHED_FIFO, SCHED_RR, SCHED_OTHER\n");
		return 1;
	}

	size = atol(argv[1]);
	nthreads = atoi(argv[2]);
	policy = parse_policy(argv[3]);

	if (size <= 0) {
		fprintf(stderr, "tamanho_buffer deve ser maior que 0\n");
		return 1;
	}
	if (nthreads < 1 || nthreads > MAX_THREADS) {
		fprintf(stderr, "num_threads deve estar entre 1 e %d\n", MAX_THREADS);
		return 1;
	}
	if (policy < 0) {
		fprintf(stderr, "politica desconhecida: %s\n", argv[3]);
		return 1;
	}

	buffer = malloc(size);
	if (!buffer) {
		perror("malloc");
		return 1;
	}
	ptr = buffer;
	buffer_end = buffer + size;

	sem_init(&mutex, 0, 1);				/* 1 = livre */
	pthread_barrier_init(&start, NULL, nthreads + 1);	/* threads + main */

	/* Tempo real (FIFO/RR) usa prioridade 1..99; as demais, prioridade 0 */
	param.sched_priority = (policy == SCHED_FIFO || policy == SCHED_RR) ? 1 : 0;

	for (i = 0; i < nthreads; i++) {
		pthread_create(&threads[i], NULL, worker, (void *)i);

		ret = pthread_setschedparam(threads[i], policy, &param);
		if (ret != 0) {
			fprintf(stderr, "pthread_setschedparam(%s): %s\n", argv[3], strerror(ret));
			return 1;
		}
	}

	/* main e o ultimo a chegar na barreira: libera a largada */
	pthread_barrier_wait(&start);

	for (i = 0; i < nthreads; i++)
		pthread_join(threads[i], NULL);

	/* Saida sem pos-processamento */
	fwrite(buffer, 1, size, stdout);
	putchar('\n');

	/* Saida apos pos-processamento */
	summarize(size, nthreads);

	pthread_barrier_destroy(&start);
	sem_destroy(&mutex);
	free(buffer);

	return 0;
}
