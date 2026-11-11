#!/bin/sh
#
# Roda os experimentos do sched_profiler no GUEST, gravando um trace por experimento.
#
# Uso:  run_experiments.sh 1      (QEMU iniciado com --smp cpus=1)
#       run_experiments.sh 2      (QEMU iniciado com --smp cpus=2)
#       run_experiments.sh 1 rr_prio fifo_idle   (so os experimentos citados)
#
# Resultados em /root/exp/<nome>.dat (trace) e /root/exp/<nome>.txt (saida do
# programa, com a linha do buffer truncada). No host, copie com pull_experiments.sh.

BUF=5000000
OUT=${OUT:-/root/exp}

# glibc nova + kernel 4.13: desliga o rseq, senao pthread_create falha
export GLIBC_TUNABLES=glibc.pthread.rseq=0

CPUS="$1"
if [ "$CPUS" != 1 ] && [ "$CPUS" != 2 ]; then
	echo "uso: $0 <1|2> [experimentos...]"
	exit 1
fi
shift

NCPU=$(grep -c ^processor /proc/cpuinfo)
if [ "$NCPU" != "$CPUS" ]; then
	echo "erro: pediu $CPUS cpu(s), mas o guest tem $NCPU. Inicie o QEMU com --smp cpus=$CPUS"
	exit 1
fi

# nome | threads | politicas
if [ "$CPUS" = 1 ]; then
	PREFIX=smp1
	LIST="
fifo_2|2|SCHED_FIFO:1
rr_2|2|SCHED_RR:1
idle_2|2|SCHED_IDLE
lowidle_2|2|SCHED_LOW_IDLE
lowidle_idle|2|SCHED_LOW_IDLE SCHED_IDLE
rr_fifo|2|SCHED_RR:10 SCHED_FIFO:10
rr_prio|2|SCHED_RR:10 SCHED_RR:20
fifo_idle|2|SCHED_FIFO:1 SCHED_IDLE
other_idle|2|SCHED_OTHER SCHED_IDLE
other_lowidle|2|SCHED_OTHER SCHED_LOW_IDLE
idle_lowidle_4|4|SCHED_IDLE SCHED_IDLE SCHED_LOW_IDLE SCHED_LOW_IDLE
"
else
	PREFIX=smp2
	LIST="
fifo_2|2|SCHED_FIFO:1
rr_2|2|SCHED_RR:1
lowidle_idle|2|SCHED_LOW_IDLE SCHED_IDLE
rr_4|4|SCHED_RR:1
idle_lowidle_4|4|SCHED_IDLE SCHED_IDLE SCHED_LOW_IDLE SCHED_LOW_IDLE
"
fi

mkdir -p "$OUT"
cd "$OUT" || exit 1

echo "$LIST" | while IFS='|' read -r name n pols; do
	[ -z "$name" ] && continue

	# filtro opcional por nome
	if [ $# -gt 0 ]; then
		found=0
		for want in "$@"; do [ "$want" = "$name" ] && found=1; done
		[ $found = 0 ] && continue
	fi

	full="${PREFIX}_${name}"
	echo "=== $full: sched_profiler $BUF $n $pols"

	trace-cmd record -o "$full.dat" -e sched:sched_switch \
		sh -c "sched_profiler $BUF $n $pols > /tmp/out.txt 2>&1" < /dev/null > /dev/null 2>&1

	# guarda a saida igual a do programa, so com a linha do buffer truncada
	awk '{ if (length($0) > 100) print substr($0, 1, 60) "..."; else print }' /tmp/out.txt > "$full.txt"
	rm -f /tmp/out.txt
	sed -n '/^Resumo:/,$p' "$full.txt" | head -n 2 | tail -n 1 | cut -c1-70
	sync
done

echo
echo "Pronto. Arquivos em $OUT:"
ls -l "$OUT"
echo "Desligue com 'poweroff' e, no host, rode custom-scripts/pull_experiments.sh"
