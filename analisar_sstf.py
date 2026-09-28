import matplotlib.pyplot as plt


def deslocamento_total(setores):
    total = 0

    for i in range(1, len(setores)):
        total += abs(setores[i] - setores[i - 1])

    return total


# Listas para armazenar as ordens
adds = []
dsps = []


# Lê o arquivo de log
with open("sstf.log", "r") as arquivo:
    for linha in arquivo:
        partes = linha.split()

        if len(partes) < 4:
            continue

        tipo = partes[1]
        setor = int(partes[3])

        if tipo == "add":
            adds.append(setor)

        elif tipo == "dsp":
            dsps.append(setor)


# Calcula os deslocamentos
deslocamento_fifo = deslocamento_total(adds)
deslocamento_sstf = deslocamento_total(dsps)


# Mostra resultados no terminal
print("Quantidade de ADD:", len(adds))
print("Quantidade de DSP:", len(dsps))

print()

print("Deslocamento FIFO:", deslocamento_fifo)
print("Deslocamento SSTF:", deslocamento_sstf)

if deslocamento_sstf > 0:
    print(
        "FIFO / SSTF:",
        round(deslocamento_fifo / deslocamento_sstf, 2)
    )

if deslocamento_fifo > 0:
    reducao = (
        (deslocamento_fifo - deslocamento_sstf)
        / deslocamento_fifo
    ) * 100

    print(
        "Redução de deslocamento:",
        round(reducao, 2),
        "%"
    )


# =========================================================
# GRÁFICO 1 - FIFO
# =========================================================

plt.figure(figsize=(12, 6))

plt.plot(
    range(1, len(adds) + 1),
    adds
)

plt.xlabel("Ordem da requisição")
plt.ylabel("Setor")
plt.title("Ordem de chegada das requisições - FIFO")

plt.grid(True)

plt.tight_layout()
plt.savefig("fifo.png", dpi=300)

plt.close()


# =========================================================
# GRÁFICO 2 - SSTF
# =========================================================

plt.figure(figsize=(12, 6))

plt.plot(
    range(1, len(dsps) + 1),
    dsps
)

plt.xlabel("Ordem da requisição")
plt.ylabel("Setor")
plt.title("Ordem de atendimento das requisições - SSTF")

plt.grid(True)

plt.tight_layout()
plt.savefig("sstf.png", dpi=300)

plt.close()


# =========================================================
# GRÁFICO 3 - COMPARAÇÃO DO DESLOCAMENTO TOTAL
# =========================================================

plt.figure(figsize=(8, 6))

nomes = ["FIFO", "SSTF"]
valores = [
    deslocamento_fifo,
    deslocamento_sstf
]

plt.bar(
    nomes,
    valores
)

plt.ylabel("Setores percorridos")
plt.title("Deslocamento total - FIFO vs SSTF")

plt.tight_layout()
plt.savefig("comparacao.png", dpi=300)

plt.close()


print()
print("Gráficos gerados:")
print("- fifo.png")
print("- sstf.png")
print("- comparacao.png")