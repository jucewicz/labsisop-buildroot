/*
 * sched_profiler: mostra o padrao de escalonamento de N threads.
 *
 * Uso:
 *   sched_profiler <tam_buffer> <num_threads> <pol1>[:prio1] [<pol2>[:prio2] ...]
 *
 *   Politicas: SCHED_LOW_IDLE, SCHED_IDLE, SCHED_FIFO, SCHED_RR, SCHED_OTHER
 *              (o prefixo "SCHED_" e opcional: RR:10 = SCHED_RR:10)
 *   Prioridade: 1..99 para FIFO/RR (padrao 1); 0 para as demais (padrao 0).
 *   Se houver menos politicas que threads, a ultima informada se repete.
 *
 * Exemplos:
 *   sched_profiler 5000000 4 SCHED_RR                 (4 threads RR, prio 1)
 *   sched_profiler 5000000 2 SCHED_RR:10 SCHED_FIFO:10
 *   sched_profiler 5000000 2 SCHED_LOW_IDLE SCHED_IDLE
 *
 * Cada thread escreve sua letra (A, B, C...) na proxima posicao livre do buffer
 * global. No fim, o buffer mostra a ordem em que o escalonador deu a CPU a cada
 * thread. O programa imprime o buffer, o resumo (letras consecutivas iguais
 * agrupadas) e, por thread: escalonamentos (blocos no buffer), caracteres
 * escritos, tempo de CPU e trocas de contexto contadas pelo kernel.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

/* Politica criada no kernel (include/uapi/linux/sched.h); a glibc nao a conhece */
#ifndef SCHED_LOW_IDLE
#define SCHED_LOW_IDLE 7
#endif

#define MAX_THREADS 26	/* uma letra por thread: A..Z */

struct thread_info {
	pthread_t tid;
	char letter;
	int policy;
	int priority;
	double cpu_ms;		/* tempo de CPU da thread (CLOCK_THREAD_CPUTIME_ID) */
	long nvcsw;		/* trocas de contexto voluntarias (dormiu: barreira, semaforo) */
	long nivcsw;		/* trocas de contexto involuntarias (preemptada) */
};

static char *buffer;			/* buffer global */
static char *ptr;			/* ponteiro global: proxima posicao livre */
static char *buffer_end;		/* primeira posicao depois do fim do buffer */
static sem_t mutex;			/* semaforo binario: protege buffer e ptr */
static pthread_barrier_t start;		/* largada: todas as threads comecam juntas */
static struct thread_info info[MAX_THREADS];

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
#define NPOLICIES (sizeof(policies) / sizeof(policies[0]))

static const char *policy_name(int policy)
{
	size_t i;

	for (i = 0; i < NPOLICIES; i++)
		if (policies[i].policy == policy)
			return policies[i].name;
	return "?";
}

/* "SCHED_RR" ou "RR" -> SCHED_RR; -1 se desconhecida */
static int parse_policy_name(const char *name)
{
	size_t i;

	for (i = 0; i < NPOLICIES; i++)
		if (strcmp(name, policies[i].name) == 0 ||
		    strcmp(name, policies[i].name + strlen("SCHED_")) == 0)
			return policies[i].policy;
	return -1;
}

/* "POLITICA[:PRIO]" -> politica e prioridade; 0 se ok, -1 se invalido */
static int parse_spec(const char *spec, int *policy, int *priority)
{
	char name[32];
	const char *colon = strchr(spec, ':');
	size_t len = colon ? (size_t)(colon - spec) : strlen(spec);
	int min, max;
	char *end;

	if (len == 0 || len >= sizeof(name)) {
		fprintf(stderr, "erro: politica invalida: '%s'\n", spec);
		return -1;
	}
	memcpy(name, spec, len);
	name[len] = '\0';

	*policy = parse_policy_name(name);
	if (*policy < 0) {
		fprintf(stderr, "erro: politica desconhecida: '%s'\n", name);
		return -1;
	}

	/* O kernel informa a faixa valida (a SCHED_LOW_IDLE so existe no kernel modificado) */
	min = sched_get_priority_min(*policy);
	max = sched_get_priority_max(*policy);
	if (min < 0 || max < 0) {
		fprintf(stderr, "erro: o kernel nao conhece %s\n", policy_name(*policy));
		return -1;
	}

	if (colon) {
		errno = 0;
		*priority = (int)strtol(colon + 1, &end, 10);
		if (errno || *end != '\0' || end == colon + 1) {
			fprintf(stderr, "erro: prioridade invalida em '%s'\n", spec);
			return -1;
		}
	} else {
		*priority = min;	/* FIFO/RR: 1; demais: 0 */
	}

	if (*priority < min || *priority > max) {
		fprintf(stderr, "erro: prioridade de %s deve estar entre %d e %d\n",
			policy_name(*policy), min, max);
		return -1;
	}
	return 0;
}

static void *worker(void *arg)
{
	struct thread_info *ti = arg;
	struct timespec ts;
	struct rusage ru;

	/* Espera todas as threads estarem criadas e com a politica aplicada */
	pthread_barrier_wait(&start);

	for (;;) {
		sem_wait(&mutex);
		if (ptr >= buffer_end) {	/* buffer cheio: devolve o semaforo e sai */
			sem_post(&mutex);
			break;
		}
		*ptr = ti->letter;
		ptr++;
		sem_post(&mutex);
	}

	/* Metricas medidas pelo kernel para esta thread */
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	ti->cpu_ms = ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
	if (getrusage(RUSAGE_THREAD, &ru) == 0) {
		ti->nvcsw = ru.ru_nvcsw;
		ti->nivcsw = ru.ru_nivcsw;
	}

	return NULL;
}

/*
 * Pos-processamento: junta letras iguais consecutivas (AAABBA -> ABA).
 * Cada bloco e um periodo em que a thread foi escalonada e escreveu.
 */
static void summarize(long size, int nthreads)
{
	int count[MAX_THREADS] = { 0 };		/* blocos (escalonamentos) por thread */
	long written[MAX_THREADS] = { 0 };	/* caracteres escritos por thread */
	double total_cpu = 0;
	char prev = 0;
	long i;

	for (i = 0; i < size; i++) {
		written[buffer[i] - 'A']++;
		if (buffer[i] != prev) {
			putchar(buffer[i]);
			count[buffer[i] - 'A']++;
			prev = buffer[i];
		}
	}
	putchar('\n');

	for (i = 0; i < nthreads; i++)
		total_cpu += info[i].cpu_ms;

	printf("\nEscalonamentos:\n");
	for (i = 0; i < nthreads; i++)
		printf("%c = %d\n", info[i].letter, count[i]);

	printf("\nEscritas no buffer:\n");
	for (i = 0; i < nthreads; i++)
		printf("%c = %ld (%.1f%%)\n", info[i].letter, written[i], 100.0 * written[i] / size);

	printf("\nTempo de CPU:\n");
	for (i = 0; i < nthreads; i++)
		printf("%c = %.1f ms (%.1f%%)\n", info[i].letter, info[i].cpu_ms,
		       total_cpu > 0 ? 100.0 * info[i].cpu_ms / total_cpu : 0.0);

	printf("\nTrocas de contexto (voluntarias / involuntarias):\n");
	for (i = 0; i < nthreads; i++)
		printf("%c = %ld / %ld\n", info[i].letter, info[i].nvcsw, info[i].nivcsw);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"uso: %s <tam_buffer> <num_threads> <pol1>[:prio1] [<pol2>[:prio2] ...]\n"
		"politicas: SCHED_LOW_IDLE, SCHED_IDLE, SCHED_FIFO, SCHED_RR, SCHED_OTHER\n"
		"prioridade: 1..99 para FIFO/RR (padrao 1), 0 para as demais\n"
		"se faltar politica para alguma thread, repete a ultima informada\n"
		"ex.: %s 5000000 2 SCHED_RR:10 SCHED_FIFO:10\n", prog, prog);
}

int main(int argc, char **argv)
{
	struct sched_param param;
	long size, i;
	int nthreads, nspecs, ret;
	char *end;

	if (argc < 4) {
		usage(argv[0]);
		return 1;
	}

	errno = 0;
	size = strtol(argv[1], &end, 10);
	if (errno || *end != '\0' || size <= 0) {
		fprintf(stderr, "erro: tam_buffer deve ser um inteiro maior que 0\n");
		return 1;
	}

	nthreads = atoi(argv[2]);
	if (nthreads < 1 || nthreads > MAX_THREADS) {
		fprintf(stderr, "erro: num_threads deve estar entre 1 e %d\n", MAX_THREADS);
		return 1;
	}

	nspecs = argc - 3;
	if (nspecs > nthreads) {
		fprintf(stderr, "erro: %d politicas para %d threads\n", nspecs, nthreads);
		return 1;
	}

	/* Politica e prioridade de cada thread; a ultima se repete se faltar */
	for (i = 0; i < nthreads; i++) {
		const char *spec = argv[3 + (i < nspecs ? i : nspecs - 1)];

		info[i].letter = 'A' + i;
		if (parse_spec(spec, &info[i].policy, &info[i].priority) < 0)
			return 1;
	}

	printf("buffer = %ld, threads = %d, cpus = %ld\n", size, nthreads, sysconf(_SC_NPROCESSORS_ONLN));
	for (i = 0; i < nthreads; i++)
		printf("%c: %s prio %d\n", info[i].letter, policy_name(info[i].policy), info[i].priority);

	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "erro ao alocar buffer\n");
		return 1;
	}
	ptr = buffer;
	buffer_end = buffer + size;

	sem_init(&mutex, 0, 1);				/* 1 = livre */
	pthread_barrier_init(&start, NULL, nthreads + 1);	/* threads + main */

	for (i = 0; i < nthreads; i++) {
		ret = pthread_create(&info[i].tid, NULL, worker, &info[i]);
		if (ret != 0) {
			fprintf(stderr, "erro ao criar thread %c: %s\n", info[i].letter, strerror(ret));
			return 1;
		}

		param.sched_priority = info[i].priority;
		ret = pthread_setschedparam(info[i].tid, info[i].policy, &param);
		if (ret != 0) {
			fprintf(stderr, "erro ao aplicar %s:%d na thread %c: %s\n",
				policy_name(info[i].policy), info[i].priority, info[i].letter, strerror(ret));
			return 1;
		}
	}

	/* A barreira so abre quando o main tambem chegar, depois de configurar todas */
	pthread_barrier_wait(&start);

	for (i = 0; i < nthreads; i++)
		pthread_join(info[i].tid, NULL);

	/* Saida sem pos-processamento */
	fwrite(buffer, 1, size, stdout);
	printf("\n\nResumo:\n");

	/* Saida apos pos-processamento */
	summarize(size, nthreads);

	pthread_barrier_destroy(&start);
	sem_destroy(&mutex);
	free(buffer);
	return 0;
}
