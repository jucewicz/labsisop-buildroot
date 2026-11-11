#!/bin/sh
#
# HOST: copia /root/exp/* do rootfs do guest para traces/exp/.
# Rode com o QEMU DESLIGADO e ANTES de qualquer 'make' (o make recria o rootfs).
#
# Uso (na raiz do repositorio):  sh custom-scripts/pull_experiments.sh

IMG=output/images/rootfs.ext2
DEST=traces/exp

command -v e2cp >/dev/null || { echo "instale: sudo apt-get install -y e2tools"; exit 1; }

mkdir -p "$DEST"
files=$(e2ls "$IMG:/root/exp" 2>/dev/null)
[ -z "$files" ] && { echo "nada em /root/exp dentro de $IMG"; exit 1; }

for f in $files; do
	e2cp "$IMG:/root/exp/$f" "$DEST/$f" && echo "copiado: $DEST/$f"
done
