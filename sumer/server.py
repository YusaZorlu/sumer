#!/usr/bin/env python3
"""Sumer's server.

Serves the browser game (the web/ folder next to this file), keeps the players' accounts and runs the
game's servers, the "rooms". Each room's world runs in a program of its own (host/sumer-host, built by
build-host.sh), started when a player joins it and stopped once it has stood empty for a while; a
survival or PvP world is saved and carries on the next time. Browsers talk to this server over
WebSockets, and it passes their messages to their room's program and back. Needs Python 3.8 or newer.

    python3 server.py                                # http://localhost:8000
    python3 server.py --port 9000 --data /opt/sumer/data

The rooms (--rooms): two Survival servers, two PvP servers and a battle royale, of which at most five run
at once (--max-active; on a small machine 2 or 3: a room that cannot start says so in the list).

Accounts keep only a name, a hash of the password (scrypt) and the game: level, K/D, the singleplayer
saves and each player's things in the survival and PvP servers. They live in DATA/sumer.db (SQLite),
copied to DATA/backups once a day (the last seven are kept). Creating an account costs the browser a
moment of work (a proof of work over SHA-256) and each address only a few a day, so no e-mail is needed
to keep out floods of accounts; addresses are kept in memory only, never written down.
"""
import argparse
import asyncio
import base64
import hashlib
import hmac
import json
import os
import re
import secrets
import signal
import socket
import sqlite3
import struct
import time
import urllib.parse

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
MAX_MESSAGE = 4 << 20
MAX_BODY = 64 << 10
SEND_BACKLOG = 400
SESSION_DAYS = 60
SAVE_SLOTS = 3
MAX_SAVE = 16384

S_HELLO, S_JOIN, S_LEAVE, S_FULL, S_NOHOST, S_TAKEN, S_HOSTLEFT, S_BUSY, S_LOGIN, S_DATA = 1, 2, 3, 4, 5, 6, 7, 8, 9, 16
S_TEST = 32
TO_SERVER, TO_ALL = 254, 255
H_STORE, H_PROGRESS, H_STATUS, H_TEST = 1, 2, 3, 4
MODES = {"survival": 0, "pvp": 1, "royale": 2}
MAX_PLAYERS = {"survival": 6, "pvp": 6, "royale": 20}
DEFAULT_ROOMS = "survival-1:survival:Survival 1,survival-2:survival:Survival 2,pvp-1:pvp:PvP 1,pvp-2:pvp:PvP 2,royale:royale:Royale"
NAME_RE = re.compile(r"^[A-Za-z0-9_.-]{3,15}$")

TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript",
    ".wasm": "application/wasm",
    ".data": "application/octet-stream",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".css": "text/css",
}

HERE = os.path.dirname(os.path.abspath(__file__))
WEB_DIR = os.path.join(HERE, "web")
ARGS = None


def log(*parts):
    print(time.strftime("%Y-%m-%d %H:%M:%S"), *parts, flush=True)


# ---- WebSockets ----

def unmask(data, mask):
    n = len(data)
    key = (mask * (n // 4 + 1))[:n]
    return (int.from_bytes(data, "little") ^ int.from_bytes(key, "little")).to_bytes(n, "little")


class Conn:
    """One WebSocket. Sends go through a queue, so a slow player never holds up the others."""

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


# ---- Accounts ----

class Store:
    """The accounts in SQLite: names, password hashes, sessions and the game kept for each."""

    def __init__(self, path):
        self.path = path
        self.db = sqlite3.connect(path)
        self.db.executescript("""
            PRAGMA journal_mode = WAL;
            CREATE TABLE IF NOT EXISTS accounts (name TEXT PRIMARY KEY COLLATE NOCASE, salt BLOB NOT NULL, hash BLOB NOT NULL,
                                                 xp INTEGER NOT NULL DEFAULT 0, kills INTEGER NOT NULL DEFAULT 0,
                                                 deaths INTEGER NOT NULL DEFAULT 0);
            CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, name TEXT NOT NULL COLLATE NOCASE, expires INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS saves (name TEXT NOT NULL COLLATE NOCASE, kind TEXT NOT NULL, data BLOB NOT NULL,
                                              PRIMARY KEY (name, kind));
        """)
        self.db.execute("DELETE FROM sessions WHERE expires < ?", (int(time.time()),))
        self.db.commit()

    def account(self, name):
        return self.db.execute("SELECT name, salt, hash, xp, kills, deaths FROM accounts WHERE name = ?", (name,)).fetchone()

    def create(self, name, salt, digest):
        try:
            self.db.execute("INSERT INTO accounts (name, salt, hash) VALUES (?, ?, ?)", (name, salt, digest))
            self.db.commit()
            return True
        except sqlite3.IntegrityError:
            return False

    def profile(self, name):
        row = self.db.execute("SELECT name, xp, kills, deaths FROM accounts WHERE name = ?", (name,)).fetchone()
        return ("%s\t%d\t%d\t%d\n" % row).encode() if row else b""

    def gain(self, name, xp, kills, deaths):
        self.db.execute("UPDATE accounts SET xp = xp + ?, kills = kills + ?, deaths = deaths + ? WHERE name = ?", (xp, kills, deaths, name))
        self.db.commit()

    def new_session(self, name):
        token = secrets.token_urlsafe(32)
        expires = int(time.time()) + SESSION_DAYS * 86400
        self.db.execute("INSERT INTO sessions (token, name, expires) VALUES (?, ?, ?)", (hashlib.sha256(token.encode()).hexdigest(), name, expires))
        self.db.commit()
        return token

    def session(self, token):
        """The account a session's token belongs to, or None."""
        if not token:
            return None
        row = self.db.execute("SELECT name, expires FROM sessions WHERE token = ?", (hashlib.sha256(token.encode()).hexdigest(),)).fetchone()
        if not row or row[1] < time.time():
            return None
        return row[0]

    def set_password(self, name, salt, digest):
        """A new password: every session the account had ends with the old one."""
        self.db.execute("UPDATE accounts SET salt = ?, hash = ? WHERE name = ?", (salt, digest, name))
        self.db.execute("DELETE FROM sessions WHERE name = ?", (name,))
        self.db.commit()

    def delete(self, name):
        """The account and everything kept with it (the daily backups lose it within a week)."""
        for table in ("accounts", "sessions", "saves"):
            self.db.execute("DELETE FROM %s WHERE name = ?" % table, (name,))
        self.db.commit()

    def end_session(self, token):
        self.db.execute("DELETE FROM sessions WHERE token = ?", (hashlib.sha256(token.encode()).hexdigest(),))
        self.db.commit()

    def load(self, name, kind):
        row = self.db.execute("SELECT data FROM saves WHERE name = ? AND kind = ?", (name, kind)).fetchone()
        return row[0] if row else b""

    def keep(self, name, kind, data):
        if data:
            self.db.execute("INSERT OR REPLACE INTO saves (name, kind, data) VALUES (?, ?, ?)", (name, kind, data))
        else:
            self.db.execute("DELETE FROM saves WHERE name = ? AND kind = ?", (name, kind))
        self.db.commit()

    def backup(self, folder, keep=7):
        os.makedirs(folder, exist_ok=True)
        path = os.path.join(folder, time.strftime("sumer-%Y-%m-%d.db"))
        if os.path.exists(path):
            return
        out = sqlite3.connect(path)
        with out:
            self.db.backup(out)
        out.close()
        old = sorted(f for f in os.listdir(folder) if f.startswith("sumer-") and f.endswith(".db"))
        for f in old[:-keep]:
            os.remove(os.path.join(folder, f))
        log("backed up the accounts to", path)


def password_hash(password, salt):
    return hashlib.scrypt(password.encode(), salt=salt, n=1 << 14, r=8, p=1, maxmem=64 << 20, dklen=32)


class Limits:
    """How often each address (or account name) may try something, counted in memory only."""

    def __init__(self):
        self.seen = {}

    def allow(self, key, most, seconds):
        if ARGS.test:
            return True
        now = time.time()
        times = [t for t in self.seen.get(key, []) if now - t < seconds]
        self.seen[key] = times
        return len(times) < most

    def note(self, key):
        self.seen.setdefault(key, []).append(time.time())

    def tidy(self):
        now = time.time()
        for key in list(self.seen):
            self.seen[key] = [t for t in self.seen[key] if now - t < 86400]
            if not self.seen[key]:
                del self.seen[key]


STORE = None
LIMITS = Limits()
CHALLENGES = {}   # challenge (hex) -> when it stops being good


def work_bits():
    return 0 if ARGS.test else ARGS.work


def work_done(challenge, name, nonce, bits):
    """SHA-256 of the challenge, the name and the nonce (4 bytes, least significant first) starts with 'bits' zero bits."""
    digest = hashlib.sha256(challenge + name.encode() + struct.pack("<I", nonce & 0xFFFFFFFF)).digest()
    value = int.from_bytes(digest, "big")
    return bits <= 0 or value >> (256 - bits) == 0


# ---- Rooms: the game's servers ----

class Guest:
    def __init__(self, conn, name, gid):
        self.conn = conn
        self.name = name
        self.gid = gid


class Room:
    def __init__(self, rid, mode, title):
        self.id = rid
        self.mode = mode
        self.title = title
        self.max = MAX_PLAYERS[mode]
        self.proc = None
        self.guests = {}
        self.empty_since = time.time()
        self.phase = 0
        self.tests = []

    def running(self):
        return self.proc is not None

    def state(self):
        if not self.running():
            return 3 if active_rooms() >= ARGS.max_active else 0
        return 2 if self.mode == "royale" and self.phase == 2 else 1

    async def start(self):
        folder = os.path.join(ARGS.data, "rooms", self.id)
        os.makedirs(folder, exist_ok=True)
        self.proc = await asyncio.create_subprocess_exec(ARGS.host_bin, self.mode, self.id, stdin=asyncio.subprocess.PIPE,
                                                         stdout=asyncio.subprocess.PIPE, cwd=folder)
        self.phase = 0
        asyncio.ensure_future(self.read(self.proc))
        log("room %s: started (%d of %d running)" % (self.id, active_rooms(), ARGS.max_active))

    def send(self, payload):
        if self.proc and self.proc.stdin and not self.proc.stdin.is_closing():
            self.proc.stdin.write(struct.pack("<I", len(payload)) + payload)

    async def stop(self):
        proc, self.proc = self.proc, None
        if not proc:
            return
        for guest in list(self.guests.values()):
            guest.conn.send(bytes([S_HOSTLEFT]))
            guest.conn.close()
        try:
            proc.stdin.close()
            await asyncio.wait_for(proc.wait(), 15)
        except (asyncio.TimeoutError, ConnectionError, OSError):
            proc.kill()
        log("room %s: stopped" % self.id)

    async def read(self, proc):
        """What the room's program says: messages for its players, and things for this server to keep."""
        try:
            while True:
                n = struct.unpack("<I", await proc.stdout.readexactly(4))[0]
                msg = await proc.stdout.readexactly(n)
                if len(msg) < 2:
                    continue
                to, body = msg[0], msg[1:]
                if to == TO_SERVER:
                    self.told(body)
                elif to == TO_ALL:
                    out = bytes([S_DATA, 0]) + body
                    for guest in list(self.guests.values()):
                        guest.conn.send(out)
                elif to in self.guests:
                    self.guests[to].conn.send(bytes([S_DATA, 0]) + body)
        except (asyncio.IncompleteReadError, ConnectionError, OSError):
            pass
        if self.proc is proc:
            # The program stopped by itself: its players are told, and the room can start again.
            self.proc = None
            log("room %s: its program stopped" % self.id)
            for guest in list(self.guests.values()):
                guest.conn.send(bytes([S_HOSTLEFT]))
                guest.conn.close()

    def told(self, body):
        kind = body[0]
        if kind in (H_STORE, H_PROGRESS) and len(body) >= 2:
            n = body[1]
            name = body[2:2 + n].decode("latin1")
            rest = body[2 + n:]
            if kind == H_STORE and self.mode != "royale" and rest:
                STORE.keep(name, self.mode, rest)
            elif kind == H_PROGRESS and len(rest) >= 8:
                xp, kills, deaths = struct.unpack("<IHH", rest[:8])
                STORE.gain(name, min(xp, 100000), kills, deaths)
        elif kind == H_STATUS and len(body) >= 3:
            self.phase = body[2]
        elif kind == H_TEST and self.tests:
            self.tests.pop(0).set_result(body[1:])


ROOMS = {}
PLAYING = {}   # account name (lower case) -> the room they are in
STARTING = None   # rooms start and stop one at a time (an asyncio.Lock)


def active_rooms():
    return sum(1 for r in ROOMS.values() if r.running())


async def serve_player(conn, room, name):
    if name is None:
        conn.send(bytes([S_LOGIN]))
        return
    if room is None:
        conn.send(bytes([S_NOHOST]))
        return
    if name.lower() in PLAYING:
        conn.send(bytes([S_TAKEN]))
        return
    if len(room.guests) >= room.max:
        conn.send(bytes([S_FULL]))
        return
    async with STARTING:
        if not room.running():
            if active_rooms() >= ARGS.max_active:
                conn.send(bytes([S_BUSY]))
                return
            try:
                await room.start()
            except OSError as e:
                log("room %s: could not start %s: %s" % (room.id, ARGS.host_bin, e))
                conn.send(bytes([S_HOSTLEFT]))
                return
    if len(room.guests) >= room.max or name.lower() in PLAYING:
        conn.send(bytes([S_FULL if len(room.guests) >= room.max else S_TAKEN]))
        return
    gid = next(i for i in range(1, room.max + 1) if i not in room.guests)
    guest = Guest(conn, name, gid)
    room.guests[gid] = guest
    PLAYING[name.lower()] = room.id
    conn.send(bytes([S_HELLO, gid]))
    things = STORE.load(name, room.mode) if room.mode != "royale" else b""
    raw = name.encode()
    room.send(bytes([S_JOIN, gid, len(raw)]) + raw + struct.pack("<H", len(things)) + things)
    log("room %s: %s joined (%d here)" % (room.id, name, len(room.guests)))
    try:
        while True:
            msg = await conn.recv()
            if msg is None:
                break
            if msg and room.proc:
                room.send(bytes([S_DATA, gid]) + msg)
                try:
                    await room.proc.stdin.drain()
                except (ConnectionError, OSError, AttributeError):
                    pass
    finally:
        if room.guests.get(gid) is guest:
            del room.guests[gid]
            room.send(bytes([S_LEAVE, gid]))
        if PLAYING.get(name.lower()) == room.id:
            del PLAYING[name.lower()]
        if not room.guests:
            room.empty_since = time.time()
        log("room %s: %s left (%d here)" % (room.id, name, len(room.guests)))


async def keep_tidy():
    """Empty rooms stop after a while; the accounts are backed up once a day."""
    while True:
        await asyncio.sleep(5)
        now = time.time()
        for room in ROOMS.values():
            if room.running() and not room.guests and now - room.empty_since > ARGS.idle:
                async with STARTING:
                    if not room.guests:
                        await room.stop()
        if int(now) % 3600 < 5:
            LIMITS.tidy()
            for c in [c for c, t in CHALLENGES.items() if t < now]:
                del CHALLENGES[c]
            STORE.backup(os.path.join(ARGS.data, "backups"))


# ---- HTTP ----

class Request:
    def __init__(self, method, url, headers, body, peer):
        self.method = method
        self.url = url
        self.headers = headers
        self.body = body
        self.query = urllib.parse.parse_qs(url.query)
        forwarded = headers.get("x-forwarded-for", "").split(",")[0].strip()
        self.ip = headers.get("cf-connecting-ip") or forwarded or (peer[0] if peer else "?")
        self.https = headers.get("x-forwarded-proto", "") == "https" or '"https"' in headers.get("cf-visitor", "")

    def cookie(self, key):
        for part in self.headers.get("cookie", "").split(";"):
            k, _, v = part.strip().partition("=")
            if k == key:
                return v
        return ""

    def account(self):
        return STORE.session(self.cookie("sumer"))


def session_cookie(req, token, days):
    secure = "; Secure" if req.https else ""
    return "sumer=%s; Path=/; Max-Age=%d; HttpOnly; SameSite=Strict%s" % (token, days * 86400, secure)


async def respond(writer, status, body=b"", kind="text/plain; charset=utf-8", head_only=False, cookie=None):
    extra = "Set-Cookie: %s\r\n" % cookie if cookie else ""
    writer.write(("HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-cache\r\n%s"
                  "Connection: close\r\n\r\n" % (status, kind, len(body), extra)).encode("latin1"))
    if not head_only:
        writer.write(body)
    await writer.drain()


def text(s):
    return s.encode()


async def api(req):
    """The account and server-list requests the title screen makes. Returns (status, body, cookie)."""
    path, method = req.url.path, req.method
    if path == "/api/rooms" and method == "GET":
        lines = ["%s\t%d\t%d\t%d\t%d\t%s" % (r.id, MODES[r.mode], len(r.guests), r.max, r.state(), r.title) for r in ROOMS.values()]
        return "200 OK", text("\n".join(lines) + "\n"), None
    if path == "/api/challenge" and method == "POST":
        if not LIMITS.allow("challenge:" + req.ip, 30, 3600):
            return "429 Too Many Requests", text("Too many tries. Wait a while and try again."), None
        LIMITS.note("challenge:" + req.ip)
        challenge = secrets.token_hex(16)
        CHALLENGES[challenge] = time.time() + 600
        return "200 OK", text("%s\t%d\n" % (challenge, work_bits())), None
    if path in ("/api/create", "/api/login") and method == "POST":
        try:
            form = json.loads(req.body.decode("utf-8"))
            name, password = str(form.get("name", "")).strip(), str(form.get("password", ""))
        except (ValueError, UnicodeDecodeError, AttributeError):
            return "400 Bad Request", text("The server could not read that."), None
        if not NAME_RE.match(name):
            return "400 Bad Request", text("An account name is 3 to 15 letters or digits (and _ - .)."), None
        if not 6 <= len(password) <= 128:
            return "400 Bad Request", text("A password needs at least 6 characters."), None
        loop = asyncio.get_event_loop()
        if path == "/api/create":
            challenge = str(form.get("challenge", ""))
            try:
                nonce = int(form.get("nonce", -1))
            except (TypeError, ValueError):
                nonce = -1
            if CHALLENGES.pop(challenge, 0) < time.time() or nonce < 0 or not work_done(bytes.fromhex(challenge), name, nonce, work_bits()):
                return "400 Bad Request", text("The new-account check did not work out. Try again."), None
            if not LIMITS.allow("create:" + req.ip, 3, 86400) or not LIMITS.allow("create", 60, 3600):
                return "429 Too Many Requests", text("Too many new accounts from here today. Try again tomorrow."), None
            if STORE.account(name):
                return "409 Conflict", text("That account name is taken."), None
            salt = secrets.token_bytes(16)
            digest = await loop.run_in_executor(None, password_hash, password, salt)
            if not STORE.create(name, salt, digest):
                return "409 Conflict", text("That account name is taken."), None
            LIMITS.note("create:" + req.ip)
            LIMITS.note("create")
            log("account %s created" % name)
            row = STORE.account(name)
        else:
            if not LIMITS.allow("login:" + req.ip, 10, 900) or not LIMITS.allow("login:" + name.lower(), 10, 900):
                return "429 Too Many Requests", text("Too many tries. Wait a few minutes and try again."), None
            row = STORE.account(name)
            digest = await loop.run_in_executor(None, password_hash, password, row[1] if row else b"0" * 16)
            if not row or not hmac.compare_digest(digest, row[2]):
                LIMITS.note("login:" + req.ip)
                LIMITS.note("login:" + name.lower())
                return "401 Unauthorized", text("Wrong account name or password."), None
        token = STORE.new_session(row[0])
        return "200 OK", STORE.profile(row[0]), session_cookie(req, token, SESSION_DAYS)
    name = req.account()
    if path == "/api/logout" and method == "POST":
        if name:
            STORE.end_session(req.cookie("sumer"))
        return "200 OK", text("bye\n"), session_cookie(req, "", 0)
    if name is None:
        return "401 Unauthorized", text("Not logged in."), None
    if path in ("/api/password", "/api/delete") and method == "POST":
        # Changing the password or deleting the account: the current password first.
        try:
            form = json.loads(req.body.decode("utf-8"))
            password, new = str(form.get("password", "")), str(form.get("newPassword", ""))
        except (ValueError, UnicodeDecodeError, AttributeError):
            return "400 Bad Request", text("The server could not read that."), None
        if not LIMITS.allow("login:" + req.ip, 10, 900) or not LIMITS.allow("login:" + name.lower(), 10, 900):
            return "429 Too Many Requests", text("Too many tries. Wait a few minutes and try again."), None
        loop = asyncio.get_event_loop()
        row = STORE.account(name)
        digest = await loop.run_in_executor(None, password_hash, password, row[1] if row else b"0" * 16)
        if not row or not hmac.compare_digest(digest, row[2]):
            LIMITS.note("login:" + req.ip)
            LIMITS.note("login:" + name.lower())
            return "401 Unauthorized", text("Wrong password."), None
        if path == "/api/password":
            if not 6 <= len(new) <= 128:
                return "400 Bad Request", text("A password needs at least 6 characters."), None
            salt = secrets.token_bytes(16)
            STORE.set_password(name, salt, await loop.run_in_executor(None, password_hash, new, salt))
            return "200 OK", STORE.profile(name), session_cookie(req, STORE.new_session(name), SESSION_DAYS)
        if name.lower() in PLAYING:
            return "409 Conflict", text("Leave the game you are playing first."), None
        STORE.delete(name)
        log("account %s deleted" % name)
        return "200 OK", text("deleted\n"), session_cookie(req, "", 0)
    if path == "/api/me" and method == "GET":
        return "200 OK", STORE.profile(name), None
    if path == "/api/saves" and method == "GET":
        out = b"".join(struct.pack("<I", len(d)) + d for d in (STORE.load(name, "solo%d" % s) for s in range(SAVE_SLOTS)))
        return "200 OK", out, None
    if path == "/api/save" and method == "POST":
        slot = req.query.get("slot", [""])[0]
        if slot not in [str(s) for s in range(SAVE_SLOTS)]:
            return "400 Bad Request", text("No such save slot."), None
        if len(req.body) > MAX_SAVE:
            return "413 Payload Too Large", text("That save is too big."), None
        STORE.keep(name, "solo" + slot, req.body)
        return "200 OK", text("kept\n"), None
    if path == "/api/progress" and method == "POST":
        # Experience gained alone, sent every few seconds: only shown, so it is only kept within reason.
        if not LIMITS.allow("progress:" + name.lower(), 4, 20):
            return "429 Too Many Requests", text("Too often."), None
        LIMITS.note("progress:" + name.lower())
        try:
            xp = int(req.body.decode().split()[0])
        except (ValueError, IndexError, UnicodeDecodeError):
            return "400 Bad Request", text("The server could not read that."), None
        STORE.gain(name, max(0, min(xp, 500)), 0, 0)
        return "200 OK", STORE.profile(name), None
    return "404 Not Found", text("not found"), None


async def test_host(req):
    """Test servers only (--test): a command for a room's program (its test build), and what it answers."""
    room = ROOMS.get(req.query.get("room", [""])[0])
    if not room or not room.running():
        return "404 Not Found", text("no such running room")
    reply = asyncio.get_event_loop().create_future()
    room.tests.append(reply)
    room.send(bytes([S_TEST]) + req.body)
    try:
        return "200 OK", await asyncio.wait_for(reply, 10)
    except asyncio.TimeoutError:
        if reply in room.tests:
            room.tests.remove(reply)
        return "504 Gateway Timeout", text("the room did not answer")


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
        length = int(headers.get("content-length", "0") or 0)
        if length > MAX_BODY:
            await respond(writer, "413 Payload Too Large", b"too big")
            return
        body = await reader.readexactly(length) if length > 0 else b""
        req = Request(method, url, headers, body, writer.get_extra_info("peername"))

        if url.path == "/ws" and "websocket" in headers.get("upgrade", "").lower():
            key = headers.get("sec-websocket-key", "")
            accept = base64.b64encode(hashlib.sha1((key + GUID).encode()).digest()).decode()
            writer.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Accept: %s\r\n\r\n" % accept).encode("latin1"))
            await writer.drain()
            conn = Conn(reader, writer)
            rid = req.query.get("room", [""])[0]
            # Test servers make rooms on demand: survival-*, pvp-*, royale-*, each suite's own.
            test_room = re.match(r"^(survival|pvp|royale)-[a-z0-9-]{1,20}$", rid)
            if ARGS.test and rid not in ROOMS and test_room:
                ROOMS[rid] = Room(rid, test_room.group(1), rid)
            await serve_player(conn, ROOMS.get(rid), req.account())
            conn.close()
            await conn.task
            return

        if url.path.startswith("/api/"):
            status, out, cookie = await api(req)
            await respond(writer, status, out, cookie=cookie)
            return
        if url.path == "/test/host" and ARGS.test and method == "POST":
            status, out = await test_host(req)
            await respond(writer, status, out)
            return
        if url.path == "/test/stop" and ARGS.test and method == "POST":
            room = ROOMS.get(req.query.get("room", [""])[0])
            if room:
                async with STARTING:
                    await room.stop()
            await respond(writer, "200 OK", b"stopped\n")
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
            data = f.read()
        kind = TYPES.get(os.path.splitext(file)[1], "application/octet-stream")
        await respond(writer, "200 OK", data, kind, head_only=method == "HEAD")
    except (asyncio.IncompleteReadError, ConnectionError, OSError):
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


def parse_rooms(spec):
    rooms = {}
    for item in spec.split(","):
        parts = [p.strip() for p in item.split(":", 2)]
        if len(parts) != 3 or parts[1] not in MODES or not re.match(r"^[a-z0-9-]{1,23}$", parts[0]):
            raise SystemExit("--rooms: each room is id:survival|pvp|royale:Title (id: lower-case letters, digits and -), not %r" % item)
        rooms[parts[0]] = Room(parts[0], parts[1], parts[2][:39])
    return rooms


async def main():
    global ARGS, STORE, ROOMS, STARTING
    parser = argparse.ArgumentParser(description="Serve Sumer to browsers, keep its accounts and run its servers.")
    parser.add_argument("--port", type=int, default=8000, help="port to listen on (default 8000)")
    parser.add_argument("--bind", default="0.0.0.0", help="address to listen on (default: all)")
    parser.add_argument("--data", default=os.path.join(HERE, "data"), help="where the accounts and the rooms' worlds are kept")
    parser.add_argument("--host-bin", default=os.path.join(HERE, "host", "sumer-host"), help="the game's server program (build-host.sh)")
    parser.add_argument("--rooms", default=DEFAULT_ROOMS, help="the servers: id:survival|pvp|royale:Title, separated by commas")
    parser.add_argument("--max-active", type=int, default=5, help="how many servers may run at once (default 5)")
    parser.add_argument("--idle", type=int, default=300, help="seconds an empty server keeps running (default 300)")
    parser.add_argument("--work", type=int, default=18, help="zero bits of work a new account costs (default 18)")
    parser.add_argument("--test", action="store_true", help="for the test suites: no work or limits, /test/host on")
    ARGS = parser.parse_args()
    ARGS.data = os.path.abspath(ARGS.data)
    ARGS.host_bin = os.path.abspath(ARGS.host_bin)

    if not os.path.isfile(os.path.join(WEB_DIR, "index.html")):
        print("No web build found in %s. Build it with: sh build-web.sh" % WEB_DIR)
        return
    if not os.access(ARGS.host_bin, os.X_OK):
        print("No game server program at %s. Build it with: sh build-host.sh" % ARGS.host_bin)
        return
    os.makedirs(ARGS.data, exist_ok=True)
    STORE = Store(os.path.join(ARGS.data, "sumer.db"))
    STORE.backup(os.path.join(ARGS.data, "backups"))
    ROOMS = parse_rooms(ARGS.rooms)
    STARTING = asyncio.Lock()
    server = await asyncio.start_server(handle, ARGS.bind, ARGS.port)
    # Stopped by the system (systemctl stop) or Ctrl+C: the rooms save their worlds first.
    stop = asyncio.Event()
    try:
        asyncio.get_event_loop().add_signal_handler(signal.SIGTERM, stop.set)
    except (NotImplementedError, AttributeError):
        pass
    asyncio.ensure_future(keep_tidy())
    print("Sumer is being served on port %d." % ARGS.port)
    print("  On this computer:  http://localhost:%d/" % ARGS.port)
    for address in addresses():
        print("  On your network:   http://%s:%d/" % (address, ARGS.port))
    print("Servers: %s (at most %d at once). Accounts in %s. Ctrl+C stops." % (", ".join(r.title for r in ROOMS.values()), ARGS.max_active, ARGS.data))
    try:
        async with server:
            await stop.wait()
    finally:
        for room in ROOMS.values():
            await room.stop()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
