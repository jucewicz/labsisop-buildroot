#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

char* buffer;
char* ptr;
char *buffer_end;   // primeira posição DEPOIS do fim do buffer
sem_t mutex;        // semáforo binário: a "caneta única"
pthread_barrier_t start;   // linha de largada

void *worker(void *arg)
{
	long id = (long)arg;
	char letter = 'A' + id;
	pthread_barrier_wait(&start);   // espera a largada
	while (1) {
		sem_wait(&mutex);            // pega a caneta (ou espera ela ficar livre)

		if (ptr >= buffer_end) {     // buffer cheio?
			sem_post(&mutex);        //   devolve a caneta antes de sair
			break;
		}

		*ptr = letter;               // escreve a letra na posição atual
		ptr++;                       // avança para a próxima posição

		sem_post(&mutex);            // devolve a caneta
	}

	return NULL;
}

void summarize(long size, int nthreads)
{
	int count[26] = { 0 };   // um contador por letra, todos começando em 0
	char prev = 0;           // a letra anterior (0 = nenhuma ainda)
	long i;

	for (i = 0; i < size; i++) {
		if (buffer[i] != prev) {          // mudou de letra = começou um bloco novo
			printf("%c", buffer[i]);       // imprime a letra uma vez
			count[buffer[i] - 'A']++;      // conta mais um bloco para essa thread
			prev = buffer[i];
		}
	}
	printf("\n");

	for (i = 0; i < nthreads; i++)
		printf("%c = %d\n", (char)('A' + i), count[i]);
}

int main(int argc, char **argv)
{
	long size;
	int nthreads;
	char *policy_name;

	if (argc != 4) {
		printf("uso: %s <tamanho_buffer> <num_threads> <politica>\n", argv[0]);
		return 1;
	}

	size = atol(argv[1]);
	nthreads = atoi(argv[2]);
	policy_name = argv[3];

	if (nthreads < 1 || nthreads > 26) {
		printf("erro: num_threads deve estar entre 1 e 26\n");
		return 1;
	}

	printf("buffer = %ld, threads = %d, politica = %s\n", size, nthreads, policy_name);

	buffer = malloc(size);
	if (!buffer) {
		printf("erro ao alocar buffer\n");
		return 1;
	}
	ptr = buffer;
	buffer_end = buffer + size;
	sem_init(&mutex, 0, 1); // inicializa o semáforo com valor 1 (caneta livre)
	pthread_barrier_init(&start, NULL, nthreads + 1);

	printf("buffer alocado em %p\n", buffer);

	pthread_t threads[nthreads];
	long i;

	for (i = 0; i < nthreads; i++)
		pthread_create(&threads[i], NULL, worker, (void *)i);

	pthread_barrier_wait(&start);   // main chega por último e libera a largada

	for (i = 0; i < nthreads; i++)
		pthread_join(threads[i], NULL);

	
	
	fwrite(buffer, 1, size, stdout);
	printf("\n\nResumo:\n");
	summarize(size, nthreads);

	
	sem_destroy(&mutex);
	pthread_barrier_destroy(&start);
	free(buffer);
	return 0;
}
