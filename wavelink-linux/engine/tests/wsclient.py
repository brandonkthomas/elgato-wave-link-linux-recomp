"""Minimal stdlib-only WebSocket client for engine tests: send JSON requests, print replies."""
import base64
import json
import os
import socket
import struct
import sys


class Client:
    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall(
            (f"GET / HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
        response = b""
        while b"\r\n\r\n" not in response:
            response += self.sock.recv(1)
        if b" 101 " not in response.split(b"\r\n")[0]:
            raise RuntimeError(response.decode(errors="replace"))
        self.next_id = 1

    def _recv_exact(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise EOFError
            buf += chunk
        return buf

    def recv(self):
        b0, b1 = self._recv_exact(2)
        length = b1 & 0x7F
        if length == 126:
            length = struct.unpack(">H", self._recv_exact(2))[0]
        elif length == 127:
            length = struct.unpack(">Q", self._recv_exact(8))[0]
        payload = self._recv_exact(length)
        return json.loads(payload.decode()) if b0 & 0x0F == 1 else None

    def send(self, obj):
        data = json.dumps(obj).encode()
        header = bytearray([0x81])
        if len(data) < 126:
            header.append(0x80 | len(data))
        elif len(data) < 65536:
            header.append(0x80 | 126)
            header += struct.pack(">H", len(data))
        else:
            header.append(0x80 | 127)
            header += struct.pack(">Q", len(data))
        mask = os.urandom(4)
        header += mask
        self.sock.sendall(bytes(header) + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def call(self, method, **params):
        rid = self.next_id
        self.next_id += 1
        self.send({"id": rid, "method": method, "params": params})
        while True:
            msg = self.recv()
            if msg and msg.get("id") == rid:
                if "error" in msg:
                    raise RuntimeError(msg["error"]["message"])
                return msg.get("result")


if __name__ == "__main__":
    client = Client(int(sys.argv[1]))
    params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
    print(json.dumps(client.call(sys.argv[2], **params)))
