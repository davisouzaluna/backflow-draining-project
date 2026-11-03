"""Assina o tópico do sensor e registra chegada, latência e tamanho de cada mensagem."""
import argparse
import csv
import json
import time

import paho.mqtt.client as mqtt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--topic", default="backflow/recife/leitura")
    ap.add_argument("--saida", default="recebimentos.csv")
    ap.add_argument("--qos", type=int, default=1)
    a = ap.parse_args()

    f = open(a.saida, "a", newline="")
    w = csv.writer(f, lineterminator="\n")
    if f.tell() == 0:
        w.writerow(["recv_ms", "seq", "pub_ms", "latencia_ms", "idade_dado_s", "bytes", "retained"])

    def on_connect(c, u, flags, rc, props=None):
        c.subscribe(a.topic, qos=a.qos)

    def on_message(c, u, m):
        recv = time.time() * 1000.0
        try:
            d = json.loads(m.payload)
            pub = d.get("pub_ms", float("nan"))
            w.writerow([f"{recv:.3f}", d.get("seq"), f"{pub:.3f}", f"{recv - pub:.3f}",
                        d.get("chuva", {}).get("idade_dado_s"), len(m.payload), int(m.retain)])
            f.flush()
        except (ValueError, TypeError):
            pass

    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    c.on_connect, c.on_message = on_connect, on_message
    c.connect(a.host, a.port, 60)
    try:
        c.loop_forever()
    except KeyboardInterrupt:
        pass
    finally:
        f.close()


if __name__ == "__main__":
    main()
