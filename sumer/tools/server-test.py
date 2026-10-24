#!/usr/bin/env python3
"""Tests server.py with the game's server program (build-host.sh first): accounts, the list of servers,
joining a room and getting its world, the things a player hands in kept with the account, a room stopping
when it stands empty and starting again where it was, and no more rooms running than --max-active allows.

    python3 tools/server-test.py
"""
import base64
import http.client
import json
import os
import shutil
import socket
import sqlite3
import struct
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S_HELLO, S_FULL, S_NOHOST, S_TAKEN, S_BUSY, S_LOGIN, S_DATA = 1, 4, 5, 6, 8, 9, 16
M_HELLO, M_WELCOME, M_RECORD, M_KICK = 1, 2, 9, 10
NET_VERSION = 15
fails = 0


def check(ok, what):
    global fails
    print(("PASS: " if ok else "FAIL: ") + what)
    if not ok:
        fails += 1


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


class Api:
    def __init__(self, port):
        self.port = port
        self.cookie = ""

    def call(self, method, path, body=b""):
        c = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        headers = {"Cookie": self.cookie} if self.cookie else {}
        c.request(method, path, body=body, headers=headers)
        r = c.getresponse()
        data = r.read()
        got = r.getheader("Set-Cookie")
        if got:
            value = got.split(";")[0]
            self.cookie = value if value != "sumer=" else ""
        c.close()
        return r.status, data

    def create(self, name, password):
        st, data = self.call("POST", "/api/challenge")
        challenge = data.decode().split("\t")[0]
        return self.call("POST", "/api/create", json.dumps({"name": name, "password": password, "challenge": challenge, "nonce": 0}).encode())

    def login(self, name, password):
        return self.call("POST", "/api/login", json.dumps({"name": name, "password": password}).encode())


class Socket:
    """A WebSocket from a player's side, enough for these tests."""

    def __init__(self, port, room, cookie):
        self.s = socket.create_connection(("127.0.0.1", port), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(("GET /ws?room=%s HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nCookie: %s\r\n\r\n" % (room, key, cookie)).encode())
        head = b""
        while b"\r\n\r\n" not in head:
            head += self.s.recv(1)
        self.buf = b""

    def send(self, payload):
        mask = os.urandom(4)
        n = len(payload)
        head = struct.pack(">BB", 0x82, 0x80 | n) if n < 126 else struct.pack(">BBH", 0x82, 0x80 | 126, n)
        self.s.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def recv(self, timeout=10):
        self.s.settimeout(timeout)
        while True:
            if len(self.buf) >= 2:
                n, at = self.buf[1] & 0x7F, 2
                if n == 126:
                    n, at = struct.unpack(">H", self.buf[2:4])[0], 4
                elif n == 127:
                    n, at = struct.unpack(">Q", self.buf[2:10])[0], 10
                if len(self.buf) >= at + n:
                    op, data = self.buf[0] & 0x0F, self.buf[at:at + n]
                    self.buf = self.buf[at + n:]
                    if op == 0x8:
                        return None
                    return data
            try:
                chunk = self.s.recv(65536)
            except socket.timeout:
                return b""
            if not chunk:
                return None
            self.buf += chunk

    def until(self, pred, timeout=10):
        end = time.time() + timeout
        while time.time() < end:
            m = self.recv(max(0.1, end - time.time()))
            if m is None:
                return None
            if m and pred(m):
                return m
        return b""

    def close(self):
        try:
            self.s.close()
        except OSError:
            pass


def record(name, slot, store_dates):
    """A player's things as the game writes them (WriteRecord in save.c)."""
    w = bytes([len(name)]) + name.encode() + bytes([slot])
    w += struct.pack("<8f", 20.0, 0.5, 30.0, 0.0, 0.3, 80.0, 100.0, 0.0)
    w += bytes([6, 10, 10, 0, 0, 0, 1, 0, 0]) + struct.pack("<H", 0) + bytes([0])
    w += bytes(24)                                   # the baskets: 12 empty slots
    w += bytes([1, store_dates]) + bytes(46)         # storage: dates in the first of 24 slots
    return w


def main():
    host = os.path.join(ROOT, "host", "sumer-host")
    if not os.access(host, os.X_OK):
        print("Build the server program first: sh build-host.sh")
        return 2
    data = tempfile.mkdtemp(prefix="sumer-test-")
    port = free_port()
    proc = subprocess.Popen([sys.executable, os.path.join(ROOT, "server.py"), "--port", str(port), "--bind", "127.0.0.1", "--data", data,
                             "--host-bin", host, "--test", "--rooms", "s1:survival:Survival 1,r1:royale:Royale", "--max-active", "1", "--idle", "2"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", port), timeout=1).close()
                break
            except OSError:
                time.sleep(0.1)
        ann, bob = Api(port), Api(port)
        st, body = ann.create("Ann", "secret1")
        check(st == 200 and body.startswith(b"Ann\t0\t0\t0") and ann.cookie.startswith("sumer="), "Ann creates an account and is logged in (%d %r)" % (st, body))
        st, body = bob.create("ann", "secret2")
        check(st == 409, "a name taken in another case is refused (%d %r)" % (st, body))
        st, body = bob.create("Bob", "secret2")
        check(st == 200, "Bob creates his")
        st, body = Api(port).login("Ann", "wrong-one")
        check(st == 401, "a wrong password is refused (%d %r)" % (st, body))
        st, body = Api(port).call("POST", "/api/create", json.dumps({"name": "Cem", "password": "secret3", "challenge": "00" * 16, "nonce": 0}).encode())
        check(st == 400, "an account without the server's challenge is refused (%d)" % st)
        st, body = ann.call("GET", "/api/me")
        check(st == 200 and body.startswith(b"Ann\t"), "Ann's browser stays logged in (%r)" % body)
        st, body = Api(port).call("GET", "/api/me")
        check(st == 401, "a browser without a session is not")
        db = sqlite3.connect(os.path.join(data, "sumer.db"))
        row = db.execute("SELECT * FROM accounts WHERE name = 'Ann'").fetchone()
        check(row is not None and b"secret1" not in row[2] and len(row[2]) == 32 and set(r[0] for r in db.execute("SELECT name FROM sessions")) == {"Ann", "Bob"},
              "the server keeps a hash of the password, never the password")

        st, body = ann.call("GET", "/api/rooms")
        rooms = [line.split("\t") for line in body.decode().strip().split("\n")]
        check(st == 200 and [r[0] for r in rooms] == ["s1", "r1"] and rooms[0][1:5] == ["0", "0", "6", "0"] and rooms[1][1:5] == ["2", "0", "20", "0"],
              "the list of servers: %s" % rooms)

        # Ann joins Survival 1: its program starts, she gets slot 1 and the world.
        a = Socket(port, "s1", ann.cookie)
        hello = a.recv()
        check(hello == bytes([S_HELLO, 1]), "Ann is let into the room (%r)" % hello)
        a.send(bytes([M_HELLO, NET_VERSION, 3]) + b"Ann")
        welcome = a.until(lambda m: m[0] == S_DATA and m[2] == M_WELCOME)
        check(bool(welcome) and welcome[3] == 1 and welcome[-9] == 0, "Ann is welcomed into slot 1, with nothing kept yet")
        snap = a.until(lambda m: m[0] == S_DATA and m[2] == 4)
        check(bool(snap) and len(snap) > 500, "and gets pictures of the world (%d bytes)" % len(snap or b""))
        st, body = ann.call("GET", "/api/rooms")
        check(body.decode().split("\n")[0].split("\t")[2:5] == ["1", "6", "1"] and body.decode().split("\n")[1].split("\t")[4] == "3",
              "the list shows her there, and the royale unable to start while one room runs (--max-active 1): %r" % body)

        again = Socket(port, "s1", ann.cookie)
        check(again.recv() == bytes([S_TAKEN]), "Ann cannot play twice at once")
        again.close()
        b = Socket(port, "r1", bob.cookie)
        check(b.recv() == bytes([S_BUSY]), "the royale cannot start: all the servers allowed are busy")
        b.close()
        anon = Socket(port, "s1", "")
        check(anon.recv() == bytes([S_LOGIN]), "a browser not logged in is turned away")
        anon.close()
        b = Socket(port, "s1", bob.cookie)
        check(b.recv() == bytes([S_HELLO, 2]), "Bob joins Ann's room")
        b.send(bytes([M_HELLO, NET_VERSION - 1, 3]) + b"Bob")
        kick = b.until(lambda m: m[0] == S_DATA and m[2] == M_KICK)
        check(bool(kick) and b"version" in kick, "a page with another version of the game is turned away")
        b.close()

        # Ann hands in her things: the server keeps them with her account.
        a.send(bytes([M_RECORD]) + record("Ann", 1, 7))
        time.sleep(0.6)
        row = db.execute("SELECT data FROM saves WHERE name = 'Ann' AND kind = 'survival'").fetchone()
        check(row is not None and len(row[0]) > 50, "Ann's things are kept with her account (%d bytes)" % (len(row[0]) if row else 0))
        a.close()

        # Empty, the room stops (and saves its world) after --idle seconds; then the royale can start.
        time.sleep(8)
        st, body = ann.call("GET", "/api/rooms")
        check(body.decode().split("\n")[0].split("\t")[4] == "0" and os.path.exists(os.path.join(data, "rooms", "s1", "sumer-mp.sav")),
              "the empty room stopped and saved its world: %r" % body)
        b = Socket(port, "r1", bob.cookie)
        check(b.recv() == bytes([S_HELLO, 1]), "now the royale starts for Bob")
        b.close()
        time.sleep(8)

        # Ann comes back: the room starts again, her things come with her.
        a = Socket(port, "s1", ann.cookie)
        a.recv()
        a.send(bytes([M_HELLO, NET_VERSION, 3]) + b"Ann")
        welcome = a.until(lambda m: m[0] == S_DATA and m[2] == M_WELCOME)
        check(bool(welcome) and welcome[3] == 1 and b"Ann" in welcome[8:] and welcome.find(bytes([1, 7])) > 0,
              "back in, Ann gets her slot and her things (7 dates in storage)")
        a.close()

        # Alone, experience is sent along; the singleplayer saves are kept too.
        st, body = ann.call("POST", "/api/progress", b"25 3 3")
        check(st == 200 and body.startswith(b"Ann\t25\t0\t0"), "experience gained alone counts, kills and deaths do not (%r)" % body)
        st, body = ann.call("POST", "/api/save?slot=1", b"SAVE" * 10)
        st2, saves = ann.call("GET", "/api/saves")
        check(st == 200 and saves == struct.pack("<I", 0) + struct.pack("<I", 40) + b"SAVE" * 10 + struct.pack("<I", 0), "a singleplayer save is kept in its slot")
        st, body = ann.call("POST", "/api/save?slot=1", b"")
        st2, saves = ann.call("GET", "/api/saves")
        check(saves == struct.pack("<III", 0, 0, 0), "an emptied slot is emptied there too")
        st, body = ann.call("POST", "/api/logout")
        st, body = ann.call("GET", "/api/me")
        check(st == 401 and not ann.cookie, "logged out")
        st, body = ann.login("ANN", "secret1")
        check(st == 200 and body.startswith(b"Ann\t25\t"), "and back in, whatever the case of the name (%r)" % body)
    finally:
        proc.terminate()
        try:
            out, _ = proc.communicate(timeout=20)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()
        if fails:
            print(out)
        shutil.rmtree(data, ignore_errors=True)
    print("%d FAILED" % fails if fails else "ALL PASSED")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
