// The connection to the relay server: a WebSocket in the browser. The relay keeps rooms of one host
// and up to five guests and forwards each message; it never looks inside them.
//
// To the host the relay sends: S_HELLO, then S_JOIN id / S_LEAVE id as guests come and go, and
// S_DATA from-id payload. To a guest: S_HELLO id, S_DATA 0 payload, or S_FULL / S_NOHOST / S_TAKEN /
// S_HOSTLEFT before closing. The host prefixes what it sends with the guest id (255: every guest).
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#endif

#define QUEUE_LEN 512

typedef struct { unsigned char *data; int len; } Queued;

static Queued queue[QUEUE_LEN];
static int head, tail;
static int linkState = LINK_OFF;
static bool asHost;

static void Drain(void)
{
    while (head != tail) {
        free(queue[head].data);
        head = (head + 1) % QUEUE_LEN;
    }
}

#if defined(__EMSCRIPTEN__)
static EMSCRIPTEN_WEBSOCKET_T sock;
static unsigned char scratch[1 << 16];

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
#endif

bool NetOpen(bool host, const char *room)
{
    NetShut();
    asHost = host;
    char query[96], url[256];
    snprintf(query, sizeof(query), "role=%s&room=%s", host ? "host" : "join", room);
    ServerAddress(url, sizeof(url), query);
#if defined(__EMSCRIPTEN__)
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
    (void)url;
    return false;
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

// The host names the guest (255 for all); a guest's messages always go to the host.
bool NetSend(int to, const void *data, int len)
{
#if defined(__EMSCRIPTEN__)
    if (linkState != LINK_OPEN || len <= 0 || len + 1 > (int)sizeof(scratch)) return false;
    if (!asHost) return emscripten_websocket_send_binary(sock, (void *)data, (uint32_t)len) == EMSCRIPTEN_RESULT_SUCCESS;
    scratch[0] = (unsigned char)to;
    memcpy(scratch + 1, data, (size_t)len);
    return emscripten_websocket_send_binary(sock, scratch, (uint32_t)len + 1) == EMSCRIPTEN_RESULT_SUCCESS;
#else
    (void)to; (void)data; (void)len;
    return false;
#endif
}

// The next message from the relay. Its data stays valid until the next call.
bool NetPoll(NetMessage *m)
{
    static unsigned char *last;
    free(last);
    last = NULL;
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
