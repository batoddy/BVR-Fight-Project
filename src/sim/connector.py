"""Tek TCP bağlantısında satır başına bir JSON mesajı gönder/al.

Bu dosya simülasyon veya kayıt yapmaz. Python sunucu, C++ istemcidir.
"""
import json
import socket
from dataclasses import asdict, dataclass

MAX_MESSAGE_BYTES = 1024 * 1024  # Satır sonu dahil en fazla 1 MiB.


@dataclass
class Message:
    kind: str
    tick: int
    data: dict

    def __post_init__(self):
        if self.kind not in ("state", "command", "end"):
            raise ValueError("Mesaj kind alanı state, command veya end olmalı.")
        if type(self.tick) is not int or self.tick < 0:
            raise ValueError("Mesaj tick alanı negatif olmayan tam sayı olmalı.")
        if not isinstance(self.data, dict):
            raise ValueError("Mesaj data alanı JSON nesnesi olmalı.")

    def encode(self):
        text = json.dumps(asdict(self), ensure_ascii=False, allow_nan=False)
        return (text + "\n").encode("utf-8")

    @classmethod
    def decode(cls, line):
        value = json.loads(line)
        if not isinstance(value, dict) or set(value) != {"kind", "tick", "data"}:
            raise ValueError("Mesaj kind, tick ve data alanlarından oluşmalı.")
        return cls(**value)


class TCPConnection:
    def __init__(self, host, port):
        self.host, self.port = host, port
        self.server = self.connection = self.reader = None

    def __enter__(self):
        """Sunucuyu aç ve tek istemcinin bağlanmasını bekle."""
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.server.bind((self.host, self.port))
            self.server.listen(1)
            print(f"C++ bağlantısı bekleniyor: {self.host}:{self.port}", flush=True)
            self.connection, address = self.server.accept()
            self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.reader = self.connection.makefile("rb")
            print(f"İstemci bağlandı: {address[0]}:{address[1]}", flush=True)
            return self
        except BaseException:
            self.close()
            raise

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def close(self):
        for resource in (self.reader, self.connection, self.server):
            if resource is not None:
                resource.close()
        self.reader = self.connection = self.server = None

    def send(self, message):
        """Mesajı tek satır JSON olarak tamamen gönder."""
        line = message.encode()
        if len(line) > MAX_MESSAGE_BYTES:
            raise ValueError("Gönderilecek mesaj 1 MiB sınırını aşıyor.")
        self.connection.sendall(line)

    def receive(self):
        """TCP parçalansa da tam satır gelene kadar bekle."""
        line = self.reader.readline(MAX_MESSAGE_BYTES + 1)
        if not line:
            raise ConnectionError("C++ bağlantısı kapandı; yeni adım uygulanmadı.")
        if len(line) > MAX_MESSAGE_BYTES or not line.endswith(b"\n"):
            raise ValueError("Mesaj tamamlanmadı veya 1 MiB sınırını aşıyor.")
        return Message.decode(line)
