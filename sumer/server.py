#!/usr/bin/env python3
"""Sumer multiplayer server.

Serves the browser build of the game (the web/ folder next to this file) and relays messages between
the player who hosts a game and up to five guests. The host's browser runs the world; this server only
forwards messages and never looks inside them. Needs nothing but Python 3.8 or newer.

    python3 server.py              # http://localhost:8000
    python3 server.py --port 9000

Everyone opens the printed address in a browser. One player presses "Host a game", the others "Join".
Add ?room=name to the address to run more than one game on the same server.
"""
import argparse
import asyncio
import base64
import hashlib
import os
import socket
import struct
import urllib.parse

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
MAX_GUESTS = 5
MAX_MESSAGE = 4 << 20
SEND_BACKLOG = 400

S_HELLO, S_JOIN, S_LEAVE, S_FULL, S_NOHOST, S_TAKEN, S_HOSTLEFT, S_DATA = 1, 2, 3, 4, 5, 6, 7, 16

TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript",
    ".wasm": "application/wasm",
    ".data": "application/octet-stream",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".css": "text/css",
}

WEB_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")


def unmask(data, mask):
    n = len(data)
    key = (mask * (n // 4 + 1))[:n]
    return (int.from_bytes(data, "little") ^ int.from_bytes(key, "little")).to_bytes(n, "little")


class Conn:
    """One WebSocket. Sends go through a queue, so a slow guest never holds up the others."""

    def __init__(self, reader, writer):
        self.reader = reader
        self.writer = writer
        self.queue = asyncio.Queue()
        self.closed = False
        self.task = asyncio.ensure_future(self.pump())

    async def pump(self):
        try:
            while True:
                frame = await self.queue.get()
                if frame is None:
                    break
                self.writer.write(frame)
                await self.writer.drain()
        except (ConnectionError, OSError):
            pass
        self.closed = True
        try:
            self.writer.close()
        except (ConnectionError, OSError):
            pass

    def frame(self, opcode, payload):
        n = len(payload)
        if n < 126:
            head = struct.pack(">BB", 0x80 | opcode, n)
        elif n < 65536:
            head = struct.pack(">BBH", 0x80 | opcode, 126, n)
        else:
            head = struct.pack(">BBQ", 0x80 | opcode, 127, n)
        return head + payload

    def send(self, payload):
        if self.closed:
            return
        if self.queue.qsize() > SEND_BACKLOG:
            self.close()
            return
        self.queue.put_nowait(self.frame(0x2, payload))

    def close(self):
        if not self.closed:
            self.closed = True
            self.queue.put_nowait(self.frame(0x8, b""))
            self.queue.put_nowait(None)

    async def recv(self):
        """The next whole binary message, or None once the other end is gone."""
        parts = []
        try:
            while True:
                head = await self.reader.readexactly(2)
                fin, opcode = head[0] & 0x80, head[0] & 0x0F
                masked, n = head[1] & 0x80, head[1] & 0x7F
                if n == 126:
                    n = struct.unpack(">H", await self.reader.readexactly(2))[0]
                elif n == 127:
                    n = struct.unpack(">Q", await self.reader.readexactly(8))[0]
                if n > MAX_MESSAGE:
                    return None
                mask = await self.reader.readexactly(4) if masked else b""
                data = await self.reader.readexactly(n) if n else b""
                if masked and n:
                    data = unmask(data, mask)
                if opcode == 0x8:
                    return None
                if opcode == 0x9:
                    if not self.closed:
                        self.queue.put_nowait(self.frame(0xA, data))
                    continue
                if opcode == 0xA:
                    continue
                parts.append(data)
                if fin:
                    return b"".join(parts)
        except (asyncio.IncompleteReadError, ConnectionError, OSError):
            return None


class Room:
    def __init__(self):
        self.host = None
        self.guests = {}


rooms = {}


async def serve_host(conn, name):
    room = rooms.setdefault(name, Room())
    if room.host is not None:
        conn.send(bytes([S_TAKEN]))
        return
    room.host = conn
    conn.send(bytes([S_HELLO, 0]))
    print("room '%s': host connected" % name)
    try:
        while True:
            msg = await conn.recv()
            if msg is None:
                break
            if len(msg) < 2:
                continue
            out = bytes([S_DATA, 0]) + msg[1:]
            if msg[0] == 255:
                for guest in list(room.guests.values()):
                    guest.send(out)
            elif msg[0] in room.guests:
                room.guests[msg[0]].send(out)
    finally:
        room.host = None
        for guest in list(room.guests.values()):
            guest.send(bytes([S_HOSTLEFT]))
            guest.close()
        room.guests.clear()
        rooms.pop(name, None)
        print("room '%s': host left, game over" % name)


async def serve_guest(conn, name):
    room = rooms.get(name)
    if room is None or room.host is None:
        conn.send(bytes([S_NOHOST]))
        return
    free = [i for i in range(1, MAX_GUESTS + 1) if i not in room.guests]
    if not free:
        conn.send(bytes([S_FULL]))
        return
    gid = free[0]
    room.guests[gid] = conn
    conn.send(bytes([S_HELLO, gid]))
    room.host.send(bytes([S_JOIN, gid]))
    print("room '%s': guest %d joined" % (name, gid))
    try:
        while True:
            msg = await conn.recv()
            if msg is None:
                break
            if room.host is not None and msg:
                room.host.send(bytes([S_DATA, gid]) + msg)
    finally:
        if room.guests.get(gid) is conn:
            del room.guests[gid]
            if room.host is not None:
                room.host.send(bytes([S_LEAVE, gid]))
            print("room '%s': guest %d left" % (name, gid))


async def respond(writer, status, body=b"", kind="text/plain; charset=utf-8", head_only=False):
    writer.write(("HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-cache\r\n"
                  "Connection: close\r\n\r\n" % (status, kind, len(body))).encode("latin1"))
    if not head_only:
        writer.write(body)
    await writer.drain()


async def handle(reader, writer):
    try:
        request = await reader.readuntil(b"\r\n\r\n")
    except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, ConnectionError, OSError):
        writer.close()
        return
    lines = request.decode("latin1").split("\r\n")
    try:
        method, target, _ = lines[0].split(" ", 2)
    except ValueError:
        writer.close()
        return
    headers = {}
    for line in lines[1:]:
        if ":" in line:
            key, value = line.split(":", 1)
            headers[key.strip().lower()] = value.strip()
    url = urllib.parse.urlsplit(target)

    try:
        if url.path == "/ws" and "websocket" in headers.get("upgrade", "").lower():
            key = headers.get("sec-websocket-key", "")
            accept = base64.b64encode(hashlib.sha1((key + GUID).encode()).digest()).decode()
            writer.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Accept: %s\r\n\r\n" % accept).encode("latin1"))
            await writer.drain()
            query = urllib.parse.parse_qs(url.query)
            role = query.get("role", ["join"])[0]
            room = query.get("room", ["sumer"])[0][:32] or "sumer"
            conn = Conn(reader, writer)
            if role == "host":
                await serve_host(conn, room)
            else:
                await serve_guest(conn, room)
            conn.close()
            await conn.task
            return

        if method not in ("GET", "HEAD"):
            await respond(writer, "405 Method Not Allowed", b"method not allowed")
            return
        path = urllib.parse.unquote(url.path)
        if path.endswith("/"):
            path += "index.html"
        file = os.path.normpath(os.path.join(WEB_DIR, path.lstrip("/")))
        if not file.startswith(WEB_DIR + os.sep) or not os.path.isfile(file):
            await respond(writer, "404 Not Found", b"not found", head_only=method == "HEAD")
            return
        with open(file, "rb") as f:
            body = f.read()
        kind = TYPES.get(os.path.splitext(file)[1], "application/octet-stream")
        await respond(writer, "200 OK", body, kind, head_only=method == "HEAD")
    except (ConnectionError, OSError):
        pass
    finally:
        try:
            writer.close()
        except (ConnectionError, OSError):
            pass


def addresses():
    found = set()
    try:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        probe.connect(("10.255.255.255", 1))
        found.add(probe.getsockname()[0])
        probe.close()
    except OSError:
        pass
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            found.add(info[4][0])
    except OSError:
        pass
    return sorted(a for a in found if not a.startswith("127."))


async def main():
    parser = argparse.ArgumentParser(description="Serve Sumer to browsers and relay multiplayer games.")
    parser.add_argument("--port", type=int, default=8000, help="port to listen on (default 8000)")
    parser.add_argument("--bind", default="0.0.0.0", help="address to listen on (default: all)")
    args = parser.parse_args()

    if not os.path.isfile(os.path.join(WEB_DIR, "index.html")):
        print("No web build found in %s. Build it with: sh build-web.sh" % WEB_DIR)
        return
    server = await asyncio.start_server(handle, args.bind, args.port)
    print("Sumer is being served on port %d." % args.port)
    print("  On this computer:  http://localhost:%d/" % args.port)
    for address in addresses():
        print("  On your network:   http://%s:%d/" % (address, args.port))
    print("One player presses 'Host a game', the others 'Join a game'. Ctrl+C stops the server.")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
