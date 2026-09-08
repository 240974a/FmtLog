#!/usr/bin/env python3
"""Плата понарошку: отдаёт поток журнала, чтобы посмотреть страницу без железа.

    python3 tools/fake-board.py

Затем откройте extras/log.html в браузере и впишите адрес, который скрипт
напечатает при запуске.

Сначала отдаётся история за несколько дней - на ней видно, как работают поиск
и ходьба по времени, - а дальше строки идут по одной, как на настоящей плате.
"""
import argparse
import http.server
import random
import time

LEVELS = 'TDIWECS'
SOURCES = ['app', 'net', 'sensor', 'db']

# Уровень, текст и границы для подставляемого числа - чтобы «очередь заполнена
# на 805%» не встречалось.
MESSAGES = [
    ('I', 'started, version 1.2.0', None),
    ('I', 'boiler {} C', (40, 75)),
    ('D', 'received {} bytes', (16, 1400)),
    ('D', 'poll took {} ms', (1, 250)),
    ('T', 'tick {}', (1, 99999)),
    ('W', 'no answer {} times in a row', (2, 9)),
    ('W', 'queue is {}% full', (70, 99)),
    ('E', 'sensor is silent', None),
    ('E', 'link lost after {} tries', (3, 12)),
    ('C', 'out of memory', None),
    ('S', 'going to sleep', None),
]


def one_line(when):
    """Строка потока: время, уровень, источник, текст - через табуляцию."""
    level, template, limits = random.choice(MESSAGES)
    text = template if limits is None else template.replace(
        '{}', str(random.randint(*limits)))
    stamp = time.strftime('%y-%m-%d %H:%M:%S', time.localtime(when))
    stamp += '.%03d' % random.randint(0, 999)
    return f'{stamp}\t{level}\t{random.choice(SOURCES)}\t{text}'


class Board(http.server.BaseHTTPRequestHandler):
    history_days = 3
    history_lines = 200
    interval = 1.0

    def log_message(self, *args):
        pass                      # не сорим в консоль на каждый запрос

    def send_stream_headers(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Cache-Control', 'no-cache')
        # Страница открыта с диска, а поток идёт отсюда - для браузера это
        # разные источники. Настоящая плата отвечает так же.
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()

    def do_GET(self):
        if self.path != '/log':
            self.send_response(404)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(b'log stream is at /log\r\n')
            return

        print(f'  подключилась вкладка {self.client_address[0]}')
        self.send_stream_headers()

        now = time.time()
        try:
            # История: строки, разбросанные по последним суткам.
            span = self.history_days * 24 * 3600
            moments = sorted(now - random.random() * span
                             for _ in range(self.history_lines))
            for when in moments:
                self.wfile.write(f'data: {one_line(when)}\n\n'.encode())
            self.wfile.flush()

            # Дальше - по одной, как на живой плате.
            while True:
                time.sleep(self.interval)
                self.wfile.write(f'data: {one_line(time.time())}\n\n'.encode())
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            print('  вкладка закрыта')


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--port', type=int, default=8080, help='порт (по умолчанию 8080)')
    p.add_argument('--days', type=int, default=3, help='за сколько дней история')
    p.add_argument('--lines', type=int, default=200, help='строк в истории')
    p.add_argument('--interval', type=float, default=1.0, help='секунд между строками')
    args = p.parse_args()

    Board.history_days = args.days
    Board.history_lines = args.lines
    Board.interval = args.interval

    server = http.server.ThreadingHTTPServer(('0.0.0.0', args.port), Board)
    print(f'плата понарошку слушает на 127.0.0.1:{args.port}')
    print(f'откройте extras/log.html и впишите адрес:  127.0.0.1:{args.port}')
    print('остановить - Ctrl+C')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\nостановлено')


if __name__ == '__main__':
    main()
