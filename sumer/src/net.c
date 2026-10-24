// The connection to server.py. In the browser it is a WebSocket; in the game's server program (host/main.c)
// it is the program's own standard input and output, which server.py holds. server.py runs every game's
// world in such a program and passes messages between it and the players' browsers; it never looks
// inside them.
//
// To a player server.py sends S_HELLO id, then S_DATA 0 payload, or S_FULL / S_NOHOST / S_TAKEN / S_BUSY /
// S_LOGIN / S_HOSTLEFT before closing. To the server program: S_JOIN id (with the account's name and its
// things), S_LEAVE id and S_DATA from-id payload. The server program puts the player's id in front of what
// it sends (255: every player; NET_TO_SERVER: server.py itself). Down the pipes every message goes with
// its length in front (4 bytes, least significant first).
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#endif

#define QUEUE_LEN 512

typedef struct { unsigned char *data; int len; } Queued;

static Queued queue[QUEUE_LEN];
static int head, tail;
static int linkState = LINK_OFF;

static void Drain(void)
{
    while (head != tail) {
        free(queue[head].data);
        head = (head + 1) % QUEUE_LEN;
    }
}

static void Push(const unsigned char *data, int len)
{
    int next = (tail + 1) % QUEUE_LEN;
    if (next == head || len <= 0) return;
    unsigned char *copy = malloc((size_t)len);
    if (!copy) return;
    memcpy(copy, data, (size_t)len);
    queue[tail] = (Queued){ copy, len };
    tail = next;
}

#if defined(__EMSCRIPTEN__)
static EMSCRIPTEN_WEBSOCKET_T sock;

static EM_BOOL OnOpen(int type, const EmscriptenWebSocketOpenEvent *e, void *user)
{
    (void)type; (void)e; (void)user;
    linkState = LINK_OPEN;
    return EM_TRUE;
}

static EM_BOOL OnMessage(int type, const EmscriptenWebSocketMessageEvent *e, void *user)
{
    (void)type; (void)user;
    if (!e->isText) Push(e->data, (int)e->numBytes);
    return EM_TRUE;
}

static EM_BOOL OnClose(int type, const EmscriptenWebSocketCloseEvent *e, void *user)
{
    (void)type; (void)e; (void)user;
    linkState = LINK_CLOSED;
    return EM_TRUE;
}

static EM_BOOL OnError(int type, const EmscriptenWebSocketErrorEvent *e, void *user)
{
    (void)type; (void)e; (void)user;
    linkState = LINK_CLOSED;
    return EM_TRUE;
}
#else
// The server program's pipes: messages in on standard input; out on what was standard output, which
// from then on goes to standard error, so nothing printed can get among the messages.
static int pipeOut = -1;
static unsigned char inBuf[1 << 17];
static int inLen;

static void ReadPipe(void)
{
    for (;;) {
        if (inLen == (int)sizeof(inBuf)) {
            linkState = LINK_CLOSED;   // a message longer than any the game sends
            return;
        }
        ssize_t n = read(0, inBuf + inLen, sizeof(inBuf) - (size_t)inLen);
        if (n == 0) {
            linkState = LINK_CLOSED;
            break;
        }
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) linkState = LINK_CLOSED;
            break;
        }
        inLen += (int)n;
    }
    int at = 0;
    while (inLen - at >= 4) {
        int len = (int)(inBuf[at] | inBuf[at + 1] << 8 | inBuf[at + 2] << 16 | (unsigned)inBuf[at + 3] << 24);
        if (len < 0 || len > (int)sizeof(inBuf) - 4) {
            linkState = LINK_CLOSED;
            return;
        }
        if (inLen - at - 4 < len || (tail + 1) % QUEUE_LEN == head) break;
        Push(inBuf + at + 4, len);
        at += 4 + len;
    }
    memmove(inBuf, inBuf + at, (size_t)(inLen - at));
    inLen -= at;
}

static bool WriteAll(const unsigned char *p, int n)
{
    while (n > 0) {
        ssize_t k = write(pipeOut, p, (size_t)n);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) {
            linkState = LINK_CLOSED;
            return false;
        }
        p += k;
        n -= (int)k;
    }
    return true;
}

// Waits until server.py sends something, at most 'seconds'.
void NetWait(double seconds)
{
    fd_set in;
    FD_ZERO(&in);
    FD_SET(0, &in);
    struct timeval tv = { (long)seconds, (long)((seconds - (double)(long)seconds) * 1e6) };
    select(1, &in, NULL, NULL, &tv);
}
#endif

// A player connects to a room; the server program opens its pipes (room is NULL there).
bool NetOpen(const char *room)
{
    NetShut();
#if defined(__EMSCRIPTEN__)
    char query[96], url[256];
    snprintf(query, sizeof(query), "room=%s", room ? room : "");
    ServerAddress(url, sizeof(url), query);
    if (!emscripten_websocket_is_supported()) return false;
    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = url;
    attr.protocols = NULL;
    attr.createOnMainThread = EM_TRUE;
    sock = emscripten_websocket_new(&attr);
    if (sock <= 0) return false;
    emscripten_websocket_set_onopen_callback(sock, NULL, OnOpen);
    emscripten_websocket_set_onmessage_callback(sock, NULL, OnMessage);
    emscripten_websocket_set_onclose_callback(sock, NULL, OnClose);
    emscripten_websocket_set_onerror_callback(sock, NULL, OnError);
    linkState = LINK_CONNECTING;
    return true;
#else
    if (room) return false;   // natively only the server program connects
    if (pipeOut < 0) {
        pipeOut = dup(1);
        dup2(2, 1);
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    }
    linkState = pipeOut >= 0 ? LINK_OPEN : LINK_CLOSED;
    return pipeOut >= 0;
#endif
}

void NetShut(void)
{
#if defined(__EMSCRIPTEN__)
    if (sock > 0) {
        emscripten_websocket_close(sock, 1000, "bye");
        emscripten_websocket_delete(sock);
        sock = 0;
    }
#endif
    Drain();
    linkState = LINK_OFF;
}

int NetLink(void)
{
    return linkState;
}

// The server program names the player (255: all of them, NET_TO_SERVER: server.py); a player's messages
// always go to the server program.
bool NetSend(int to, const void *data, int len)
{
    if (linkState != LINK_OPEN || len <= 0 || len > (1 << 16)) return false;
#if defined(__EMSCRIPTEN__)
    (void)to;
    return emscripten_websocket_send_binary(sock, (void *)data, (uint32_t)len) == EMSCRIPTEN_RESULT_SUCCESS;
#else
    unsigned char head[5] = { (unsigned char)(len + 1), (unsigned char)((len + 1) >> 8), (unsigned char)((len + 1) >> 16), 0,
                              (unsigned char)to };
    return WriteAll(head, 5) && WriteAll(data, len);
#endif
}

// The next message from server.py. Its data stays valid until the next call.
bool NetPoll(NetMessage *m)
{
    static unsigned char *last;
    free(last);
    last = NULL;
#if !defined(__EMSCRIPTEN__)
    if (head == tail && linkState == LINK_OPEN) ReadPipe();
#endif
    if (head == tail) return false;
    Queued q = queue[head];
    head = (head + 1) % QUEUE_LEN;
    last = q.data;
    *m = (NetMessage){ q.data[0], 0, q.data + 1, q.len - 1 };
    if (m->kind == S_DATA || m->kind == S_JOIN || m->kind == S_LEAVE || m->kind == S_HELLO) {
        if (m->len < 1) {
            m->kind = 0;
            return true;
        }
        m->from = m->data[0];
        m->data++;
        m->len--;
    }
    return true;
}
