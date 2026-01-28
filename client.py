import socket
import struct

OP_INSERT = 1
OP_CONTAINS = 2

def req(op: int, key: bytes) -> bytes:
    payload = struct.pack("!B H", op, len(key)) + key
    return struct.pack("!I", len(payload)) + payload

def read_resp(sock: socket.socket) -> int:
    hdr = sock.recv(4)
    if len(hdr) < 4:
        raise RuntimeError("short header")
    (n,) = struct.unpack("!I", hdr)
    body = b""
    while len(body) < n:
        chunk = sock.recv(n - len(body))
        if not chunk:
            raise RuntimeError("eof")
        body += chunk
    if n != 1:
        raise RuntimeError("bad resp len")
    return body[0]

s = socket.create_connection(("127.0.0.1", 9000))

s.sendall(req(OP_INSERT, b"apple"))
print(read_resp(s))  # 1

s.sendall(req(OP_CONTAINS, b"apple"))
print(read_resp(s))  # 1

s.sendall(req(OP_CONTAINS, b"banana"))
print(read_resp(s))  # 0

s.sendall(req(OP_INSERT, b"apple"))
print(read_resp(s))  # 0

s.close()
