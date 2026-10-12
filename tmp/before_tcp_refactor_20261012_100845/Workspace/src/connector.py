import json
import socket
from dataclasses import dataclass, asdict


@dataclass
class Message:
    kind: str
    tick: int
    data: dict

    def encode(self):
        # Tek satır JSON; satır sonu mesajın bittiğini belirtir.
        text = json.dumps(asdict(self), allow_nan=False)
        return (text + "\n").encode("utf-8")

    @classmethod
    def decode(cls, line):
        return cls(**json.loads(line))


def run_connection(simulation):
    with socket.socket() as server:
        server.bind(("127.0.0.1", 6000))
        server.listen(1)

        print("C++ bağlantısı bekleniyor...")
        connection, _ = server.accept()

        with connection, connection.makefile("rb") as reader:
            while not simulation.finished():
                # 1. Simülasyonun mevcut ölçümünü gönder.
                state = simulation.read_state()
                message = Message("state", state["tick"], state)
                connection.sendall(message.encode())

                # 2. C++ komutunu bekle ve tamamını oku.
                line = reader.readline(1024 * 1024)
                if not line:
                    raise ConnectionError("C++ bağlantısı kapandı.")
                if not line.endswith(b"\n"):
                    raise ValueError("Mesaj tamamlanmadı veya çok büyük.")

                command = Message.decode(line)

                # 3. Komut bu ölçüme ait olmalı.
                if command.kind != "command" or command.tick != message.tick:
                    raise ValueError("Komut türü veya adım numarası yanlış.")

                # 4. Komutu uygula; simülasyonu bir adım ilerlet.
                simulation.step({
                    "based_on_tick": command.tick,
                    "aircraft": command.data["aircraft"],
                })

            connection.sendall(
                Message("end", simulation.tick, {}).encode()
            )