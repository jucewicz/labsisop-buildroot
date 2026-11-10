# sched_profiler: explicação dos resultados

Explicação do código, do relatório e do que aconteceu em cada política, baseada nos traces reais (`traces/*.dat`) e nas saídas (`traces/*.txt`).

---

## 1. Resumo em 1 minuto

| Política | Resumo | A/B/C/D | Duração | O que aconteceu |
|---|---|---|---|---|
| FIFO | `A` | 1/0/0/0 | ~1,1 s | A pegou a CPU e só largou quando o buffer encheu |
| RR | `ABCDABCDABCDAB` | 4/4/3/3 | ~1,8 s | revezamento em ordem fixa, fatias de 100 ms |
| IDLE | `BCDABDCABC...` | 119/121/125/124 | ~2,8 s | troca a cada ~4 ms, sem ordem fixa |
| LOW_IDLE | `BCDABCDABC...` | 100/95/101/92 | ~2,1 s | igual ao IDLE |

Parâmetros de todas as execuções: buffer de 5.000.000, 4 threads, QEMU com 1 CPU.

**A ideia central:**

- **FIFO e RR** são de **tempo real**: passam na frente de tudo e geram **poucos blocos grandes**. A diferença entre as duas é a **fatia de tempo**: a RR tem (100 ms), a FIFO não.
- **IDLE e LOW_IDLE** são do **CFS** (o escalonador "justo"): trocam de thread a todo momento e geram **centenas de blocos pequenos**, dividindo a CPU por igual.

---

## 2. O código em 6 partes

| Parte | Linhas | O que faz |
|---|---|---|
| `#define SCHED_LOW_IDLE 7` | 9–11 | a glibc não conhece a sua política, então o programa define o número que o kernel usa |
| Globais | 13–17 | `buffer` (o "caderno"), `ptr` (próxima posição livre), `buffer_end` (fim), `mutex` (semáforo = "caneta única"), `start` (barreira = "linha de largada") |
| `worker` | 19–39 | cada thread espera a largada e repete: pega o semáforo → buffer cheio? sai → escreve a letra → avança `ptr` → devolve o semáforo |
| `summarize` | 41–58 | junta letras repetidas (`AAABBA` → `ABA`) e conta os blocos de cada letra = quantas vezes cada thread rodou |
| `parse_policy` | 60–68 | transforma o texto `"SCHED_RR"` no número da política |
| `main` | 70–144 | lê os argumentos, aloca o buffer, cria as threads, aplica a política em cada uma, libera a largada, espera todas (`pthread_join`) e imprime |

**Três detalhes importantes:**

1. **Semáforo:** sem ele, uma thread interrompida no meio de `*ptr = letter; ptr++;` faria outra escrever na mesma posição, e letras sumiriam. Isso acontece mesmo com 1 CPU, porque a troca de thread pode acontecer em qualquer instrução.
2. **`sem_post` antes do `break`:** se a thread saísse segurando o semáforo, as outras esperariam para sempre (*deadlock*).
3. **Barreira com `nthreads + 1`:** as N threads mais o `main`. Ninguém começa a escrever antes de todas estarem criadas e com a política certa.

---

## 3. Como ler o trace

Cada linha do `sched_switch` diz "quem saiu da CPU ==> quem entrou":

```text
sched_profiler:172 [98] R ==> sched_profiler:173 [98]
```

| Campo | Significado |
|---|---|
| `172`, `173` | PID da thread. O `main` é o primeiro PID; A, B, C e D são os 4 seguintes |
| `[98]` | tempo real com prioridade 1 (o kernel mostra 99 − 1 = 98) |
| `[120]` | prioridade normal; IDLE e LOW_IDLE também aparecem assim |
| `R` | saiu **contra a vontade**: ainda queria CPU (preempção) |
| `S` | saiu **para dormir**: foi esperar algo (barreira, semáforo, `join`) |
| `x` | a thread **terminou** |
| `D` | esperando o **disco** (o `main` gravando o arquivo `.txt`) |

PIDs de cada execução: RR (main 171, A–D 172–175), FIFO (186, 187–190), IDLE (194, 195–198), LOW_IDLE (201, 202–205).

---

## 4. Cada política: o fluxo e o porquê

Os tempos estão contados **a partir da largada** (quando a barreira abre).

### SCHED_FIFO: `A` sozinho

**Fluxo:**

1. O `main` (normal) cria o **A** e aplica FIFO. O A é tempo real, então **passa na frente do `main` na hora**, roda até a barreira e dorme (`S`). O mesmo acontece com B, C e D, um de cada vez. Por isso elas chegam na barreira **na ordem A, B, C, D**.
2. O `main` chega por último e **abre a barreira**. As 4 acordam na ordem da fila. O A, primeiro da fila, **passa na frente do `main`** (que fica `R`).
3. **0 a 951 ms:** o A escreve sem parar. B, C e D esperam: a FIFO não tem fatia de tempo, e o A nunca bloqueia.
4. **951 ms:** o **RT throttling** entra. O Linux deixa o tempo real usar no máximo 950 ms por segundo. Um `kworker` roda, o `main` aproveita para entrar no `pthread_join` e dormir (`S`), e a CPU fica **ociosa por ~51 ms**.
5. **~1003 ms:** o A volta (continua na frente da fila) e roda até **~1115 ms**, quando o buffer enche. Então ele termina (`x`).
6. B, C e D rodam **microssegundos** cada: veem o buffer cheio e terminam (`x`).
7. O `main` acorda do `join`, grava o arquivo (estado `D`) e termina.

**Por quê:** na FIFO, quem pega a CPU **só larga quando termina ou bloqueia**. O A nunca bloqueia, porque o semáforo está sempre livre (ninguém mais está rodando). E por que o A, e não outra letra? Com 1 CPU, cada thread FIFO passa na frente do `main` assim que é criada, então chega na barreira na ordem de criação.

### SCHED_RR: `ABCDABCDABCDAB`

**Fluxo:**

1. A largada é igual à da FIFO: A, B, C e D chegam na barreira em ordem.
2. Revezamento em fatias de **100 ms**, sempre com `R` (preempção ao fim da fatia):

| Tempo (ms) | Quem roda | Observação |
|---|---|---|
| 0 – 97 | A | |
| 97 – 297 | **B (2 fatias)** | a fatia de B acabou **com B segurando o semáforo**: C, D e A tentaram pegar, dormiram (`S`) em menos de 1 ms, e a CPU voltou para o B |
| 297 – 397 | C | |
| 397 – 498 | D | |
| 498 – 697 | **A (2 fatias)** | mesmo efeito do semáforo |
| 697 – 797 | B | |
| 797 – 897 | C | |
| 897 – 1045 | D | no meio, **RT throttling**: ~48 ms de CPU ociosa, e o `main` entra no `join` |
| 1045 – 1145 | A | |
| 1145 – 1246 | B | |
| 1246 – 1545 | **C (3 fatias)** | mesmo efeito do semáforo, duas vezes seguidas |
| 1545 – 1645 | D | |
| 1645 – 1745 | A | |
| 1745 – 1824 | B | o buffer enche; B termina (`x`), depois C, D e A |

3. O `main` acorda, grava o arquivo e termina.

**Por quê:** a RR é uma FIFO **com fatia de tempo**. Quando a fatia acaba, a thread vai para o fim da fila, por isso a ordem é sempre A → B → C → D. Alguns blocos são maiores (B, A e C com 2 ou 3 fatias) por causa do semáforo: se a fatia acaba com a thread **segurando o semáforo**, as outras tentam pegá-lo, dormem, e a única thread pronta é a que o segura, que então ganha outra fatia.

Os blocos também têm **quantidades de letras diferentes** em tempos iguais (por exemplo, C escreveu 111 mil letras em 100 ms, e D escreveu 445 mil em 100 ms). A causa provável é o custo do semáforo: quando há threads dormindo esperando por ele, cada `sem_post` precisa de uma chamada ao kernel para acordar alguém, e a escrita fica mais lenta.

### SCHED_IDLE: centenas de blocos pequenos

**Fluxo:**

1. As threads IDLE têm prioridade **menor que a do `main`**, então **nenhuma roda durante a criação**. O `main` cria as 4, chega na barreira **primeiro** e dorme (`S`).
2. Agora as threads rodam, uma de cada vez: **A** chega na barreira (`S`), depois **B**, **C**, e **D por último**. O D abre a barreira.
3. O D acorda todo mundo, inclusive o `main`. O `main` é normal, então **passa na frente do D** (D fica `R`), entra no `pthread_join` e dorme.
4. O CFS escolhe a próxima: o **B**, e não o A. O CFS escolhe quem **usou menos CPU** (menor *vruntime*), e o A tinha gastado um pouco mais (~0,2 ms contra ~0,01 ms dos outros) antes da barreira.
5. Daí em diante, **trocas a cada ~4 ms** (o "tick" do kernel, 250 por segundo), em ordem variável:
   - **669 trocas com `R`**: o tick chegou e o CFS passou a vez para quem tinha usado menos CPU;
   - **510 trocas com `S`**: a thread tentou pegar o semáforo, que estava com outra, e dormiu;
   - **~35 vezes** outras tarefas (`trace-cmd`, `kworker`) passaram na frente, porque IDLE perde para qualquer tarefa normal.
6. Divisão da CPU: A 28%, B 24%, C 23%, D 23%, ou seja, praticamente igual. O buffer enche em ~2,8 s e as threads terminam.

**Por quê:** a IDLE é escalonada pelo **CFS**, que não usa fila de chegada nem fatia fixa. Ele sempre escolhe **quem usou menos CPU até agora**, então todo mundo recebe a mesma parte, e as trocas são frequentes e sem ordem fixa.

### SCHED_LOW_IDLE: igual ao IDLE

**Fluxo:** idêntico ao do IDLE. O `main` cria as threads sem que elas rodem, chega primeiro na barreira, o D (último) abre, o `main` entra no `join`, e o **B** começa. Depois:

- **506 trocas com `R`** e **347 com `S`**, a cada ~4 ms;
- ~24 vezes outras tarefas passaram na frente;
- divisão da CPU de ~25% para cada thread, e ~2,1 s no total.

**Por quê é igual ao IDLE:** no seu kernel, a LOW_IDLE é o CFS com **peso 1** (o menor possível; a IDLE tem peso 3) e **perde a CPU para qualquer tarefa que não seja LOW_IDLE**. Mas aqui as 4 threads são **todas LOW_IDLE**, com o mesmo peso. Entre iguais, o CFS divide por igual, exatamente como no IDLE. A diferença só apareceria **competindo com tarefas de outras políticas**, por exemplo uma thread IDLE contra uma LOW_IDLE.

A diferença de duração (2,1 s contra 2,8 s) e de número de trocas não deve ser interpretada: foi **uma execução de cada**, e o emulador varia de uma execução para outra.

---

## 5. O relatório, seção por seção

| Seção | O que diz | Confere com os traces? |
|---|---|---|
| 1. Implementação | o código: buffer, semáforo, barreira, políticas, resumo | ✅ |
| 2. Ambiente | QEMU, 1 CPU, 5 milhões, 4 threads, comando do `trace-cmd` | ✅ (a duração real varia de ~1,1 s a ~2,8 s, conforme a política) |
| 3. Saída de exemplo | RR: blocos de 481.916 A, 634.758 B, 111.065 C... | ✅ números iguais aos do `rr.txt` |
| 4. Tabela | resumos e contagens das 4 políticas | ✅ "~500" e "~670" trocas = trocas com `R` (506 e 669) |
| 5. Análise | FIFO sem fatia + throttling; RR circular + efeito do semáforo; IDLE/LOW_IDLE pelo CFS e por que são iguais | ✅ tudo bate com os traces |
| 6. Figuras | KernelShark das 4 políticas | ✅ verde = rodando, vermelho = pronta, preto = dormindo |

**Se o professor perguntar:**

- *"Por que o semáforo, com 1 CPU?"* A troca de thread pode cair no meio do `*ptr = letter; ptr++`.
- *"Por que a FIFO foi só o A?"* Sem fatia de tempo, e com 1 CPU as threads chegam na largada em ordem.
- *"Por que a RR não é perfeitamente ABCD?"* É, na ordem; os blocos maiores vêm de fatias seguidas quando a thread é preemptada segurando o semáforo.
- *"Por que IDLE e LOW_IDLE deram igual?"* Todas as threads têm a mesma política e o mesmo peso; a LOW_IDLE só se diferencia competindo com outras políticas.
- *"O que é a pausa de ~50 ms?"* RT throttling: o tempo real só pode usar 950 ms de cada 1 s.
