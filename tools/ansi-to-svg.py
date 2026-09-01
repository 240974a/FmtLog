#!/usr/bin/env python3
"""Рисует снимок терминала в SVG по цветному выводу журнала.

На вход - строки с управляющими последовательностями ANSI, на выход - SVG,
который одинаково виден в любом просмотрщике Markdown.
"""
import re, sys, html

# палитра xterm-256: нужные нам номера
PALETTE = {
    15: '#ffffff', 28: '#008700', 33: '#0087ff', 40: '#00d700',
    45: '#00d7ff', 93: '#8700ff', 124: '#af0000', 214: '#ffaf00',
    238: '#444444', 247: '#9e9e9e',
}
BG, FG = '#0d1117', '#c9d1d9'
CW, LH, PAD = 8.42, 21, 16   # ширина знака, высота строки, поле

SEQ = re.compile(r'\033\[(?:38;5;(\d+)|0)m')

def parse(line):
    """Разбирает строку на куски (текст, цвет)."""
    out, pos, color = [], 0, None
    for m in SEQ.finditer(line):
        if m.start() > pos:
            out.append((line[pos:m.start()], color))
        color = PALETTE.get(int(m.group(1))) if m.group(1) else None
        pos = m.end()
    if pos < len(line):
        out.append((line[pos:], color))
    return out

lines = [l.rstrip('\n') for l in sys.stdin if l.strip()]
rows = [parse(l) for l in lines]
width = max(sum(len(t) for t, _ in r) for r in rows) * CW + PAD * 2
height = len(rows) * LH + PAD * 2

print(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.0f}" height="{height:.0f}" '
      f'viewBox="0 0 {width:.0f} {height:.0f}" font-family="ui-monospace,SFMono-Regular,'
      f'Menlo,Consolas,monospace" font-size="14">')
print(f'<rect width="100%" height="100%" rx="6" fill="{BG}"/>')
for i, row in enumerate(rows):
    y = PAD + (i + 1) * LH - 6
    x = PAD
    print(f'<text y="{y}" xml:space="preserve">', end='')
    for text, color in row:
        fill = color or FG
        print(f'<tspan x="{x:.1f}" fill="{fill}">{html.escape(text)}</tspan>', end='')
        x += len(text) * CW
    print('</text>')
print('</svg>')
