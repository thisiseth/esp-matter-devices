# LLM generated script for play chime call 

import base64
import hashlib
import json
import os
import socket
import struct
import uuid
import sys


HOST = "core-matter-server"
PORT = 5580
PATH = "/ws"

NODE_ID = int(sys.argv[1])


payload = {
    "message_id": uuid.uuid4().hex,
    "command": "device_command",
    "args": {
        "node_id": NODE_ID,
        "endpoint_id": 1,
        "cluster_id": 1366,
        "command_name": "PlayChimeSound",
        "payload": {
        }
    }
}


def recv_exact(sock, length):
    data = b""

    while len(data) < length:
        chunk = sock.recv(length - len(data))

        if not chunk:
            raise ConnectionError("WebSocket connection closed")

        data += chunk

    return data


def receive_frame(sock):
    first, second = recv_exact(sock, 2)

    opcode = first & 0x0F
    masked = bool(second & 0x80)
    length = second & 0x7F

    if length == 126:
        length = struct.unpack("!H", recv_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", recv_exact(sock, 8))[0]

    mask = recv_exact(sock, 4) if masked else None
    data = recv_exact(sock, length)

    if mask:
        data = bytes(byte ^ mask[i % 4] for i, byte in enumerate(data))

    return opcode, data


def send_text(sock, text):
    data = text.encode()
    mask = os.urandom(4)
    masked_data = bytes(byte ^ mask[i % 4] for i, byte in enumerate(data))

    frame = bytearray([0x81])  # FIN + text frame

    if len(data) < 126:
        frame.append(0x80 | len(data))
    elif len(data) < 65536:
        frame.append(0x80 | 126)
        frame.extend(struct.pack("!H", len(data)))
    else:
        frame.append(0x80 | 127)
        frame.extend(struct.pack("!Q", len(data)))

    frame.extend(mask)
    frame.extend(masked_data)

    sock.sendall(frame)


with socket.create_connection((HOST, PORT), timeout=10) as sock:
    key = base64.b64encode(os.urandom(16)).decode()

    request = (
        f"GET {PATH} HTTP/1.1\r\n"
        f"Host: {HOST}:{PORT}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n"
    )

    sock.sendall(request.encode())

    response = b""
    while b"\r\n\r\n" not in response:
        response += sock.recv(4096)

    if b" 101 " not in response.split(b"\r\n", 1)[0]:
        raise RuntimeError(response.decode(errors="replace"))

    expected_accept = base64.b64encode(
        hashlib.sha1(
            (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()
        ).digest()
    )

    if expected_accept not in response:
        raise RuntimeError("Invalid WebSocket handshake response")

    # Matter Server normally sends an initial server-info frame.
    opcode, data = receive_frame(sock)
    print("Initial:", data.decode(errors="replace"))

    send_text(sock, json.dumps(payload))

    while True:
        opcode, data = receive_frame(sock)

        if opcode == 0x1:  # text
            message = json.loads(data)
            print(message)

            if message.get("message_id") == payload["message_id"]:
                break

        elif opcode == 0x8:  # close
            break
