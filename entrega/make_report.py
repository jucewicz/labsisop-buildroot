"""Gera entrega/relatorio_sched_profiler.pdf (2 paginas, duas colunas).

Uso (na raiz do repositorio):  python3 entrega/make_report.py
Le as saidas de traces/exp/*.txt e os prints de entrega/prints/*.png.
"""
import os
import re
import tempfile
from PIL import Image as PILImage
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import cm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_JUSTIFY, TA_CENTER
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import registerFontFamily
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                Image, Preformatted, FrameBreak, KeepTogether, PageBreak)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXP = os.path.join(ROOT, 'traces', 'exp')
OUT = os.path.join(ROOT, 'entrega', 'relatorio_sched_profiler.pdf')
TMP = tempfile.mkdtemp()

F = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Sans', F + 'DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('Sans-B', F + 'DejaVuSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Mono', F + 'DejaVuSansMono.ttf'))
registerFontFamily('Sans', normal='Sans', bold='Sans-B', italic='Sans', boldItalic='Sans-B')

W, H = A4
M = 1.2 * cm
GAP = 0.55 * cm
COLW = (W - 2 * M - GAP) / 2
FULLW = W - 2 * M
TITLE_H = 1.35 * cm

body = ParagraphStyle('b', fontName='Sans', fontSize=8.7, leading=11.3, alignment=TA_JUSTIFY, spaceAfter=2.5)
note = ParagraphStyle('n', parent=body, fontSize=7.0, leading=8.7)
h = ParagraphStyle('h', fontName='Sans-B', fontSize=8.8, leading=10.5, spaceBefore=3, spaceAfter=1.5)
title = ParagraphStyle('t', fontName='Sans-B', fontSize=13, leading=16, alignment=TA_CENTER)
sub = ParagraphStyle('s', fontName='Sans', fontSize=8, leading=10, alignment=TA_CENTER, textColor=colors.HexColor('#444444'))
code = ParagraphStyle('c', fontName='Mono', fontSize=6.6, leading=8.0, backColor=colors.HexColor('#f2f2f2'),
                      borderPadding=2.5, leftIndent=2.5, rightIndent=2.5, spaceBefore=2, spaceAfter=5)
OUT_FS = 5.7
out_box = ParagraphStyle('ob', fontName='Mono', fontSize=OUT_FS, leading=6.7, backColor=colors.HexColor('#f2f2f2'),
                         borderPadding=2.5, leftIndent=2.5, rightIndent=2.5, spaceBefore=4)
cap = ParagraphStyle('cap', fontName='Sans', fontSize=6.9, leading=8.3, alignment=TA_CENTER, spaceBefore=3, spaceAfter=8)
cell = ParagraphStyle('cell', fontName='Sans', fontSize=6.3, leading=7.4)
cellb = ParagraphStyle('cellb', parent=cell, fontName='Sans-B')


def P(t, st=body):
    return Paragraph(t, st)


def C(t):
    return f'<font name="Mono">{t}</font>'


# ---------------------------------------------------------------- dados das execucoes
def read_exp(name):
    with open(os.path.join(EXP, name + '.txt')) as f:
        text = f.read()

    def section(title):
        part = text.split(title + ':', 1)[1].strip().split('\n\n')[0]
        return [l for l in part.split('\n') if re.match(r'^[A-Z] = ', l)]

    pols = re.findall(r'^[A-Z]: SCHED_(\S+) prio (\d+)', text, re.M)
    blocks = sum(int(l.split('= ')[1]) for l in section('Escalonamentos'))
    cpu = [re.search(r'\(([\d.]+)%\)', l).group(1) for l in section('Tempo de CPU')]
    wr = [re.search(r'\(([\d.]+)%\)', l).group(1) for l in section('Escritas no buffer')]
    return pols, blocks, cpu, wr


def faithful_output(name, width):
    """Saida como o script salvou (linhas longas ja truncadas); quebra linhas maiores que a coluna."""
    with open(os.path.join(EXP, name + '.txt')) as f:
        lines = f.read().rstrip('\n').split('\n')
    out = []
    for l in lines:
        out += [l[i:i + width] for i in range(0, len(l), width)] or ['']
    return '\n'.join(out)


SHORT = {'LOW_IDLE': 'LI', 'IDLE': 'ID', 'FIFO': 'FF', 'RR': 'RR', 'OTHER': 'OT'}


def config(pols):
    parts = []
    for p, prio in pols:
        s = SHORT[p] + (f':{prio}' if p in ('FIFO', 'RR') else '')
        if parts and parts[-1][0] == s:
            parts[-1][1] += 1
        else:
            parts.append([s, 1])
    return ' '.join(s if n == 1 else f'{n}×{s}' for s, n in parts)


def pct(vals):
    return '/'.join(str(round(float(v))) for v in vals)


# duracao de cada execucao (s), medida nos traces (traces/exp/*.dat)
DUR = {'smp1_fifo_2': 1.23, 'smp1_rr_2': 1.90, 'smp1_idle_2': 1.59, 'smp1_lowidle_2': 1.53,
       'smp1_lowidle_idle': 1.55, 'smp1_rr_fifo': 1.19, 'smp1_rr_prio': 1.45, 'smp1_fifo_idle': 1.17,
       'smp1_other_idle': 1.27, 'smp1_other_lowidle': 1.30, 'smp1_idle_lowidle_4': 1.79,
       'smp2_fifo_2': 4.77, 'smp2_rr_2': 4.87, 'smp2_lowidle_idle': 5.04, 'smp2_rr_4': 8.01,
       'smp2_idle_lowidle_4': 5.71}
ORDER = list(DUR)

# ---------------------------------------------------------------- texto
story = []
story.append(Paragraph('Laboratório de Sistemas Operacionais – sched_profiler', title))
story.append(Paragraph('Calvin Silva, João Medeiros, Pedro Jucewicz · PUCRS · novembro de 2026', sub))
story.append(FrameBreak())

story.append(Paragraph('1. Programa e metodologia', h))
story.append(Preformatted('sched_profiler <tam_buffer> <num_threads>\n'
                          '               <pol1>[:prio1] [<pol2>[:prio2] ...]', code))
story.append(P(
    'Cada thread recebe sua política e prioridade (se faltarem, a última se repete) e escreve sua letra no buffer global '
    f'sob um semáforo binário; uma barreira ({C("pthread_barrier_t")}, N+1 participantes) garante que todas começam '
    f'juntas, já com a política aplicada. {C("SCHED_LOW_IDLE")} vale 7, como no kernel. Além do buffer e do resumo, o '
    'programa imprime por thread os <b>escalonamentos</b> (blocos no buffer), os <b>caracteres escritos</b>, o '
    '<b>tempo de CPU</b> e as <b>trocas de contexto</b> voluntárias/involuntárias; as duas últimas vêm do kernel e não '
    'dependem do semáforo.'))
story.append(P(
    '<b>Ambiente.</b> Host: GitHub Codespace de 2 núcleos (AMD EPYC 9V74, 2 vCPUs), 7,8 GiB de RAM, Ubuntu 24.04. '
    'Guest: QEMU 8.2.2 (qemu-system-i386, emulação por software, sem KVM), 128 MiB de RAM (padrão), 1 ou 2 vCPUs '
    f'({C("--smp cpus=1|2")}), distribuição Buildroot com Linux 4.13.9 e {C("SCHED_LOW_IDLE")} (peso 1, preemptada ao acordar qualquer tarefa de outra '
    f'política), com {C("WMULT_LOW_IDLEPRIO")} corrigido para 2<super>32</super>/1 (antes era 2<super>32</super>/3, o '
    'inverso do peso da IDLE). Buffer de 5.000.000 posições; 11 experimentos com 1 núcleo e 5 com 2 núcleos, cada um '
    f'com {C("trace-cmd record -e sched:sched_switch")} durante a execução.'))
story.append(P(
    '<i>Nota:</i> o script dos experimentos salva a saída truncando linhas com mais de 100 caracteres (o buffer e os '
    'resumos longos) para 60 caracteres e “...”. Isso ocorre só no arquivo salvo, após a execução; os valores por thread '
    'são calculados pelo programa sobre o buffer completo e não são afetados.', note))

# tabela de resultados
rows = [['Exp.', 'Threads', 'Dur. (s)', 'Blocos', 'CPU (%)', 'Escritas (%)']]
for i, name in enumerate(ORDER):
    pols, blocks, cpu, wr = read_exp(name)
    label = f'{"1c" if name.startswith("smp1") else "2c"}·{i + 1 if i < 11 else i - 10}'
    b = f'{blocks / 1000:.0f} mil' if blocks >= 10000 else str(blocks)
    rows.append([label, config(pols), f'{DUR[name]:.2f}', b, pct(cpu), pct(wr)])
data = [[Paragraph(c, cellb if r == 0 else cell) for c in row] for r, row in enumerate(rows)]
t = Table(data, colWidths=[0.95 * cm, 2.3 * cm, 0.9 * cm, 1.15 * cm, 1.6 * cm, COLW - 6.9 * cm], repeatRows=1)
t.setStyle(TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e6e6e6')),
    ('BACKGROUND', (0, 12), (-1, -1), colors.HexColor('#f4f7fb')),
    ('GRID', (0, 0), (-1, -1), 0.3, colors.HexColor('#999999')),
    ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
    ('TOPPADDING', (0, 0), (-1, -1), 1.2), ('BOTTOMPADDING', (0, 0), (-1, -1), 1.2),
    ('LEFTPADDING', (0, 0), (-1, -1), 2.5), ('RIGHTPADDING', (0, 0), (-1, -1), 2.5),
]))
story.append(KeepTogether([
    Paragraph('2. Resultados', h), t, Spacer(1, 2),
    P('1c/2c = 1 ou 2 núcleos. FF = FIFO, RR, OT = OTHER, ID = IDLE, LI = LOW_IDLE (FIFO/RR com a prioridade após “:”). '
      'CPU e escritas por thread na ordem A/B/C/D; blocos = total de escalonamentos.', note)]))

story.append(Paragraph('3. O que os experimentos mostraram', h))
story.append(P(
    '<b>Mesma política, 1 núcleo.</b> FIFO (1c·1): a primeira thread a receber a CPU escreve 100% e só a larga ao '
    'terminar, pois não há fatia de tempo. RR (1c·2): as threads se revezam em fatias de ~100 ms (resumo ABABABABABAB). '
    'IDLE e LOW_IDLE (1c·3, 1c·4): o CFS divide ~50/50, trocando a cada ~4 ms (tick de 250 Hz) em ordem variável. Com '
    'pesos iguais, IDLE e LOW_IDLE são indistinguíveis.'))
story.append(P(
    '<b>Políticas diferentes, 1 núcleo.</b> <i>Tempo real × CFS:</i> uma FIFO contra uma IDLE (1c·8) deixa a IDLE com 0% '
    'das escritas. <i>RR × FIFO de mesma prioridade</i> (1c·6, Fig. 3): A (RR) roda uma única fatia de ~97 ms, vai ao fim '
    'da fila e B (FIFO) não devolve mais a CPU (7,5% × 92,5%); a FIFO só é interrompida pelo <i>RT throttling</i> '
    '(950 ms por período de 1 s), quando a CPU fica ~50 ms com as tarefas normais (o <i>main</i> entra no pthread_join) e '
    'depois ociosa. <i>Pesos no CFS:</i> OTHER × IDLE (1c·9) dá 99,7/0,3% de CPU, exatamente a razão 1024:3; OTHER × '
    'LOW_IDLE (1c·10) deixa a LOW_IDLE com 0,6% (acima dos 0,1% do peso, provavelmente pelo crédito de vruntime ao acordar, '
    'relevante em execução tão curta).'))
story.append(P(
    '<b>LOW_IDLE × IDLE.</b> No mesmo núcleo a IDLE recebe <b>69%</b> da CPU e a LOW_IDLE <b>31%</b> '
    '(1c·5, Fig. 1; o peso 1:3 prevê 75/25); com duas de cada (1c·11, Fig. 2), ~34,5% para cada IDLE e ~15,5% para cada '
    'LOW_IDLE (previsto 37,5/12,5). O trace mostra os dois mecanismos da política: (1) quando a IDLE acorda (sem_post da '
    'LOW_IDLE), a LOW_IDLE é preemptada no mesmo instante (estado R ~0,1 ms após a troca anterior); (2) a LOW_IDLE só '
    'recupera a CPU no tick, quando a IDLE esgota sua fatia, ou quando a IDLE dorme no semáforo. A distância da teoria vem '
    'do tick: a fatia ideal da LOW_IDLE (~1,5 ms) vira 4 ms na prática, enquanto a IDLE emenda até 3 ticks (12 ms).'))
story.append(P(
    '<b>Prioridades.</b> Com RR:10 × RR:20 (1c·7) a thread de prioridade 20 escreve 100% e a de prioridade 10 nunca recebe '
    'fatia: o revezamento do RR só ocorre entre threads de <i>mesma</i> prioridade, e a de maior prioridade sempre passa à '
    'frente. Entre tempo real e CFS a prioridade nem entra em jogo: qualquer FIFO/RR passa à frente de OTHER, IDLE e '
    'LOW_IDLE.'))
story.append(P(
    '<b>1 × 2 núcleos.</b> Com uma CPU por thread a política deixa de importar: FIFO (2c·1, Fig. 5), RR (2c·2) e LOW_IDLE × '
    'IDLE (2c·3, Fig. 4) dividem ~50/50, e as threads mudam de CPU durante a execução. A diferença de peso só reaparece com '
    'mais threads que núcleos (2c·5: IDLE ~29%, LOW_IDLE ~21%). O paralelismo não ajuda este programa: só uma thread '
    'escreve por vez, então elas disputam o semáforo, dormem e acordam milhares de vezes (trocas voluntárias passam de '
    'dezenas para milhares) e a execução fica ~4× mais lenta (FIFO: 1,2 s × 4,8 s). O buffer passa a alternar quase letra a '
    'letra (centenas de milhares de blocos): com várias CPUs, contar blocos mede a disputa pelo semáforo, não o '
    'escalonador, e o tempo de CPU passa a ser a métrica confiável.'))
story.append(P(
    '<b>Conclusão.</b> No mesmo núcleo, a política define quem roda: tempo real passa à frente de tudo, a prioridade '
    'decide entre tempo real, a fatia separa RR de FIFO, e no CFS a CPU segue o peso, com a SCHED_LOW_IDLE recebendo menos '
    'que a SCHED_IDLE, como projetado. Com núcleos suficientes para todas as threads, essas diferenças desaparecem.'))

# ---------------------------------------------------------------- pagina 2: prints + saidas
story.append(PageBreak())
story.append(Paragraph('4. KernelShark e saídas do programa', h))
story.append(P('Em cada figura: linhas CPU (tarefa em execução) e uma linha por thread (verde = executando, vermelho = '
               'pronta, preto = dormindo); o primeiro PID é o <i>main</i>, os seguintes são A, B, C... Abaixo, a saída '
               'salva da mesma execução.', note))

figs = [
    ('smp1_lowidle_idle', 0.41, 'Fig. 1 – 1c·5, A = LOW_IDLE, B = IDLE (main = 169, A = 170, B = 171): B executa mais e '
                                'por mais tempo a cada vez.'),
    ('smp1_idle_lowidle_4', 0.53, 'Fig. 2 – 1c·11, A,B = IDLE, C,D = LOW_IDLE (main = 241, A–D = 242–245): as linhas '
                                  'de C e D têm menos verde.'),
    ('smp1_rr_fifo', 0.41, 'Fig. 3 – 1c·6, A = RR:10, B = FIFO:10 (main = 181, A = 182, B = 183): uma fatia de A, B até '
                           'o fim, pausa do RT throttling.'),
    ('smp2_lowidle_idle', 0.47, 'Fig. 4 – 2c·3, A = LOW_IDLE, B = IDLE (main = 152, A = 153, B = 154), trecho de ~3 s: '
                                'as duas executam o tempo todo.'),
    ('smp2_fifo_2', 0.47, 'Fig. 5 – 2c·1, 2× FIFO:1 (main = 126, A = 127, B = 128): as duas em paralelo, em CPUs '
                          'diferentes.'),
]
for k, (name, bottom, caption) in enumerate(figs):
    im = PILImage.open(os.path.join(ROOT, 'entrega', 'prints', name + '.png'))
    w, hh = im.size
    crop = os.path.join(TMP, name + '.png')
    im.crop((0, int(0.105 * hh), w, int(bottom * hh))).save(crop)
    cw, ch = PILImage.open(crop).size
    img = Image(crop, width=COLW, height=COLW * ch / cw)
    chars = int((COLW - 10) / (0.602 * OUT_FS))
    out = Preformatted(faithful_output(name, chars), out_box)
    story.append(KeepTogether([img, out, Paragraph(caption, cap)]))
    if k == 1:
        story.append(FrameBreak())


# ---------------------------------------------------------------- layout
def frames_first():
    top = H - M - TITLE_H
    z = dict(leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    return [Frame(M, top, FULLW, TITLE_H, id='title', **z),
            Frame(M, M, COLW, top - M, id='c1', **z),
            Frame(M + COLW + GAP, M, COLW, top - M, id='c2', **z)]


def frames_later():
    z = dict(leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    return [Frame(M, M, COLW, H - 2 * M, id='l1', **z),
            Frame(M + COLW + GAP, M, COLW, H - 2 * M, id='l2', **z)]


def decor(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor('#cccccc'))
    x = M + COLW + GAP / 2
    canvas.line(x, M, x, H - M - (TITLE_H if doc.page == 1 else 0))
    canvas.setFont('Sans', 7)
    canvas.drawRightString(W - M, M / 2, str(doc.page))
    canvas.restoreState()


doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=M, rightMargin=M, topMargin=M, bottomMargin=M,
                      title='Laboratório de Sistemas Operacionais – sched_profiler',
                      author='Calvin Silva, João Medeiros, Pedro Jucewicz')
doc.addPageTemplates([PageTemplate(id='first', frames=frames_first(), onPage=decor, autoNextPageTemplate='later'),
                      PageTemplate(id='later', frames=frames_later(), onPage=decor)])
doc.build(story)

from pypdf import PdfReader
print('paginas:', len(PdfReader(OUT).pages))
