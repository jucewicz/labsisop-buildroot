"""Gera entrega/relatorio_sched_profiler.pdf (2 paginas, duas colunas).

Uso (na raiz do repositorio):  python3 entrega/make_report.py
Le os resultados de traces/*.txt e os prints de entrega/prints/*.png.
"""
import os
import re
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
OUT = os.path.join(ROOT, 'entrega', 'relatorio_sched_profiler.pdf')
import tempfile
TMP = tempfile.mkdtemp()

F = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Sans', F + 'DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('Sans-B', F + 'DejaVuSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Mono', F + 'DejaVuSansMono.ttf'))
registerFontFamily('Sans', normal='Sans', bold='Sans-B', italic='Sans', boldItalic='Sans-B')

W, H = A4
M = 1.3 * cm
GAP = 0.6 * cm
COLW = (W - 2 * M - GAP) / 2
FULLW = W - 2 * M
TITLE_H = 1.5 * cm

body = ParagraphStyle('b', fontName='Sans', fontSize=8.6, leading=11.2, alignment=TA_JUSTIFY, spaceAfter=3)
h = ParagraphStyle('h', fontName='Sans-B', fontSize=9, leading=11, spaceBefore=4, spaceAfter=2)
title = ParagraphStyle('t', fontName='Sans-B', fontSize=13, leading=16, alignment=TA_CENTER)
sub = ParagraphStyle('s', fontName='Sans', fontSize=8, leading=10, alignment=TA_CENTER, textColor=colors.HexColor('#444444'))
code = ParagraphStyle('c', fontName='Mono', fontSize=7.0, leading=8.6, backColor=colors.HexColor('#f2f2f2'),
                      borderPadding=3, leftIndent=3, rightIndent=3, spaceBefore=2, spaceAfter=6)
out_style = ParagraphStyle('o', fontName='Mono', fontSize=6.0, leading=7.0)
cap = ParagraphStyle('cap', fontName='Sans', fontSize=7.6, leading=9.2, alignment=TA_CENTER, spaceBefore=2, spaceAfter=9)
cell = ParagraphStyle('cell', fontName='Sans', fontSize=7.0, leading=8.6)
cellb = ParagraphStyle('cellb', parent=cell, fontName='Sans-B')


def P(t):
    return Paragraph(t, body)


def C(t):
    return f'<font name="Mono">{t}</font>'


def read_run(name):
    """Le traces/<name>.txt e devolve (cabecalho, inicio do buffer, resumo, escalonamentos, escritas)."""
    with open(os.path.join(ROOT, 'traces', name + '.txt')) as f:
        lines = f.read().split('\n')
    i = lines.index('Resumo:')
    resumo = lines[i + 1]
    esc = [l for l in lines[lines.index('Escalonamentos:') + 1:] if re.match(r'^[A-Z] = ', l)][:4]
    wr = [l for l in lines[lines.index('Escritas no buffer:') + 1:] if re.match(r'^[A-Z] = ', l)][:4]
    return lines[0], lines[2][:1], resumo, esc, wr


def faithful_output(name, width):
    """Saida exatamente como o programa imprime; so a linha do buffer e truncada,
    e linhas maiores que a coluna quebram como num terminal."""
    with open(os.path.join(ROOT, 'traces', name + '.txt')) as f:
        lines = f.read().rstrip('\n').split('\n')
    out = lines[:2] + [lines[2][:width - 3] + '...']
    for l in lines[3:]:
        out += [l[i:i + width] for i in range(0, len(l), width)] or ['']
    return '\n'.join(out)


OUT_FS = 6.0
story = []
story.append(Paragraph('Laboratório de Sistemas Operacionais – sched_profiler', title))
story.append(Paragraph('Calvin Silva, João Medeiros, Pedro Jucewicz · PUCRS · novembro de 2026', sub))
story.append(FrameBreak())

story.append(Paragraph('1. Implementação', h))
story.append(P(
    f'<b>Laço de cada thread.</b> A thread converte seu índice em letra ({C("&#39;A&#39; + id")}) e repete: '
    f'{C("sem_wait")}; se {C("ptr &gt;= buffer_end")}, faz {C("sem_post")} e sai; senão escreve a letra, avança o '
    f'ponteiro e faz {C("sem_post")}. O teste de fim fica <i>dentro</i> da seção crítica: fora dela, duas threads poderiam '
    f'ver a última posição livre e uma escreveria além do buffer. O {C("sem_post")} antes do {C("break")} evita que a '
    'thread termine segurando o semáforo, o que deixaria as demais bloqueadas para sempre.'))
story.append(P(
    f'<b>Política e largada.</b> As threads são criadas com a política padrão e só depois recebem a política pedida '
    f'({C("pthread_setschedparam")}, prioridade 1 para FIFO/RR e 0 para IDLE/LOW_IDLE). Para que nenhuma thread escreva '
    f'nesse intervalo, todas esperam numa barreira ({C("pthread_barrier_t")}) com N+1 participantes: ela só abre quando o '
    '<i>main</i> também chega, e ele só chega depois de configurar todas. Sem ela, a thread A começaria a escrever enquanto '
    'as outras ainda nem existem, e o buffer refletiria a ordem de criação, não o escalonador. Se o kernel recusar a política, '
    f'o programa aborta, para não medir com a política errada. {C("SCHED_LOW_IDLE")} é definida no código com o valor 7, '
    'o mesmo do kernel modificado, pois a glibc não a conhece.'))
story.append(P(
    '<b>Pós-processamento.</b> Uma única passada no buffer conta, para cada letra, as posições escritas e os blocos (uma '
    'troca de letra inicia um bloco novo, que é impresso no resumo). O número de blocos é o que o programa chama de '
    '“escalonamentos”.'))

story.append(Paragraph('2. Ambiente e parâmetros', h))
story.append(P(
    f'QEMU i386 com <b>1 núcleo</b> ({C("--smp cpus=1")}), kernel Linux 4.13.9 com a política {C("SCHED_LOW_IDLE")}, '
    'distribuição gerada com Buildroot. Parâmetros: <b>buffer de 5.000.000 posições e 4 threads</b> (cerca de 1 a 2,5 s de '
    'execução no emulador, tempo suficiente para várias fatias de 100 ms do RR). Cada política foi executada uma vez, com o '
    'trace capturado durante a execução:'))
story.append(Preformatted('trace-cmd record -o rr.dat -e sched:sched_switch \\\n'
                          '  sh -c "sched_profiler 5000000 4 SCHED_RR > rr.txt"', code))

story.append(Paragraph('3. Saída de exemplo (SCHED_RR, buffer truncado)', h))
story.append(Preformatted(faithful_output('rr', int((COLW - 8) / (0.602 * 7.0))), code))

rows = [
    ['Política', 'Resumo (início)', 'Escalon.\nA/B/C/D', 'Escritas (%)\nA/B/C/D', 'Troca de thread'],
    ['FIFO', 'A', '1/0/0/0', '100/0/0/0', 'nenhuma'],
    ['RR', 'ABCDAB', '4/4/4/4', '48/23/9/20', '~100 ms, ordem fixa'],
    ['IDLE', 'CDBACD', '103/92/98/98', '27/21/27/25', '~4 ms, aleatória'],
    ['LOW_IDLE', 'BCDABC', '109/114/113/115', '24/26/26/24', '~4 ms, aleatória'],
]
data = [[Paragraph(c.replace('\n', '<br/>'), cellb if i == 0 else cell) for c in r] for i, r in enumerate(rows)]
t = Table(data, colWidths=[1.55 * cm, 1.45 * cm, 2.55 * cm, 1.8 * cm, COLW - 7.35 * cm])
t.setStyle(TableStyle([
    ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e6e6e6')),
    ('GRID', (0, 0), (-1, -1), 0.4, colors.HexColor('#999999')),
    ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
    ('TOPPADDING', (0, 0), (-1, -1), 2), ('BOTTOMPADDING', (0, 0), (-1, -1), 2),
    ('LEFTPADDING', (0, 0), (-1, -1), 3), ('RIGHTPADDING', (0, 0), (-1, -1), 3),
]))
story.append(KeepTogether([Paragraph('4. Resultados', h), t, Spacer(1, 4), P('“Escalonamentos” conta os blocos de cada letra no buffer: fatias consecutivas da mesma thread formam um '
               'único bloco. As entradas reais na CPU aparecem no trace.')]))

story.append(Paragraph('5. Análise', h))
story.append(P(
    '<b>SCHED_FIFO.</b> As threads FIFO têm prioridade sobre o <i>main</i> (normal): cada uma, ao receber a política, toma '
    'a CPU, chega à barreira e dorme. Assim elas chegam na ordem A, B, C, D. Quando o <i>main</i> libera a barreira, A está '
    'na frente da fila e, como a FIFO não tem fatia de tempo e A nunca bloqueia (o semáforo está sempre livre), executa '
    'sozinha por cerca de 1,1 s até encher o buffer (resumo <b>A</b>, 100% das escritas). Nesta execução não houve pausa '
    'do RT throttling (limite de 950 ms por período de 1 s). Depois, B, C e D executam por microssegundos, encontram o '
    'buffer cheio e terminam (estado x).'))
story.append(P(
    '<b>SCHED_RR.</b> A largada é igual à da FIFO, mas cada thread recebe uma fatia de ~100 ms e volta ao fim da fila '
    '(trocas com estado R), gerando a ordem circular A, B, C, D. O RR é justo nas fatias, mas não no trabalho: quando a '
    'fatia termina com a thread dentro da seção crítica, as demais tentam o semáforo, dormem (S) e a CPU volta à mesma '
    'thread, que ganha uma fatia extra (A teve ~796 ms de CPU e D ~313 ms). Além disso, enquanto há threads acordadas que '
    'ainda não reexecutaram, cada sem_post precisa de uma chamada ao kernel (futex), reduzindo a velocidade de escrita; '
    'isso explica, provavelmente, C ter escrito só 8,5% do buffer com ~400 ms de CPU. O RT throttling gerou duas pausas de '
    '~50 ms.'))
story.append(P(
    '<b>SCHED_IDLE e SCHED_LOW_IDLE.</b> Aqui as threads têm prioridade <i>inferior</i> à do <i>main</i>: nenhuma executa '
    'durante a criação, o <i>main</i> chega primeiro à barreira e a última thread (D) é quem a libera. Ao acordar, o '
    '<i>main</i> preempta D (tarefa normal sempre preempta uma ociosa), entra no pthread_join e dorme. O CFS escolhe '
    'sempre a thread com menor vruntime; com peso muito baixo (3 no IDLE, 1 no LOW_IDLE), poucos microssegundos de CPU '
    'antes da barreira já decidem quem começa (C no IDLE, B no LOW_IDLE). As trocas ocorrem a cada ~4 ms (tick de '
    '250 Hz), sem ordem fixa, e a CPU é dividida de forma equilibrada (21% a 27% das escritas por thread). Os padrões são '
    'semelhantes porque todas as threads usam a mesma política: a SCHED_LOW_IDLE só se diferencia ao competir com tarefas '
    'de outras políticas.'))
story.append(P(
    '<b>Conclusão.</b> As políticas de tempo real (FIFO, RR) passam à frente das tarefas normais e geram poucos blocos '
    'longos, diferenciando-se pela fatia de tempo; as políticas do CFS (IDLE, LOW_IDLE) geram centenas de blocos curtos e '
    'uma divisão equilibrada da CPU. A quantidade escrita por thread também depende do semáforo, e não só do escalonador.'))

story.append(PageBreak())
story.append(Paragraph('6. Saídas do programa e visualização no KernelShark', h))
story.append(P('Em cima, o trace (evento sched:sched_switch). Em cada linha de thread: verde = executando, retângulo '
               'vermelho = pronta (na fila), linha preta = dormindo; o primeiro PID é o <i>main</i> e os quatro seguintes '
               'são A a D. Embaixo, a saída do sched_profiler na mesma execução, como impressa pelo programa (só a '
               'linha do buffer foi truncada; o buffer completo está em traces/*.txt).'))
story.append(FrameBreak())

out_box = ParagraphStyle('ob', parent=out_style, backColor=colors.HexColor('#f2f2f2'), borderPadding=3,
                         leftIndent=3, rightIndent=3, spaceBefore=6)
figs = [('fifo', 'SCHED_FIFO', 'Figura 1 – SCHED_FIFO (main = 123, A–D = 124–127): A executa sozinha até encher o buffer; '
                               'B, C e D só aparecem na largada e ao terminar.'),
        ('rr', 'SCHED_RR', 'Figura 2 – SCHED_RR (main = 116, A–D = 117–120): fatias de ~100 ms em ordem circular fixa.'),
        ('idle', 'SCHED_IDLE', 'Figura 3 – SCHED_IDLE (main = 130, A–D = 131–134): trocas a cada ~4 ms, sem ordem fixa; '
                               'o main só executa no início e no fim.'),
        ('lowidle', 'SCHED_LOW_IDLE', 'Figura 4 – SCHED_LOW_IDLE (main = 137, A–D = 138–141): padrão semelhante ao '
                                      'SCHED_IDLE.')]
for k, (name, policy, caption) in enumerate(figs):
    im = PILImage.open(os.path.join(ROOT, 'entrega', 'prints', name + '.png'))
    w, hh = im.size
    crop = os.path.join(TMP, name + '.png')
    im.crop((0, int(0.105 * hh), w, int(0.532 * hh))).save(crop)
    cw, ch = PILImage.open(crop).size
    img = Image(crop, width=COLW, height=COLW * ch / cw)
    chars = int((COLW - 12) / (0.602 * OUT_FS))
    out = Preformatted(faithful_output(name, chars), out_box)
    story.append(KeepTogether([img, out, Spacer(1, 4), Paragraph(caption, cap)]))
    if k == 1:
        story.append(FrameBreak())


def frames_first():
    top = H - M - TITLE_H
    z = dict(leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    return [Frame(M, top, FULLW, TITLE_H, id='title', **z),
            Frame(M, M, COLW, top - M, id='c1', **z),
            Frame(M + COLW + GAP, M, COLW, top - M, id='c2', **z)]


TOPH = 2.3 * cm


def frames_later():
    z = dict(leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    return [Frame(M, H - M - TOPH, FULLW, TOPH, id='top', **z),
            Frame(M, M, COLW, H - 2 * M - TOPH, id='l1', **z),
            Frame(M + COLW + GAP, M, COLW, H - 2 * M - TOPH, id='l2', **z)]


def decor(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor('#cccccc'))
    x = M + COLW + GAP / 2
    canvas.line(x, M, x, H - M - (TITLE_H if doc.page == 1 else TOPH))
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
