"""Servidor HTTP que imita o formato da APAC (so para testar o pipeline)."""
import argparse
import json
import time
from http.server import BaseHTTPRequestHandler, HTTPServer


def corpo(cadencia_s, atraso_s, estacoes=4, base=0.0):
    t = int(time.time()) - atraso_s
    t -= t % cadencia_s
    dh = time.strftime("%Y-%m-%d %H:%M", time.localtime(t))
    itens = []
    for i in range(estacoes):
        reg = {"cidade": "RECIFE", "chuva": round(base + 0.5 * i, 1), "dataHora": dh}
        itens.append({"Dados_completos": json.dumps(reg)})
    itens.append({"Dados_completos": json.dumps({"cidade": "OLINDA", "chuva": 9.9, "dataHora": dh})})
    return json.dumps(itens).encode()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8099)
    ap.add_argument("--cadencia", type=int, default=600)
    ap.add_argument("--atraso", type=int, default=120)
    ap.add_argument("--chuva", type=float, default=0.0, help="chuva base (mm) somada a 0,5*i por estacao")
    a = ap.parse_args()

    class H(BaseHTTPRequestHandler):
        def do_GET(self):
            b = corpo(a.cadencia, a.atraso, base=a.chuva)
            self.send_response(200)
            self.send_header("Content-Length", str(len(b)))
            self.end_headers()
            self.wfile.write(b)

        def log_message(self, *args):
            pass

    HTTPServer(("127.0.0.1", a.port), H).serve_forever()


if __name__ == "__main__":
    main()
