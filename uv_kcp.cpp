// libuv+kcp
// g++ -o uv_kcp{,.cpp} -g -O3 -std=c++11 -Wall -pthread -I/home/chuqq/temp/libuv/libuv_bin/include -L/home/chuqq/temp/libuv/libuv_bin/lib -luv
// g++ -o uv_kcp{,.cpp} -g -O3 -std=c++11 -Wall -pthread -luv
// export LD_LIBRARY_PATH=../libuv/libuv_bin/lib
// ./uv_kcp
 
#include <uv.h>
#include "ikcp.c"

#include <thread>
#include <cassert>
#include <queue>
#include <vector>

#include <time.h>
#include <unistd.h>

static inline unsigned long long nstime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((unsigned long long)ts.tv_sec)*1e9 + ts.tv_nsec;
}

#define log(fmt, args...) 0
// #define log(fmt, args...) log(fmt, ##args)

static unsigned long long starttime;

/////////////////////////////////////////////////////////////////////////////////

static uv_loop_t client_loop;
static uv_udp_t client_udp;
// static auto ikcp_cmp = [](ikcpcb *left, ikcpcb *right) { return ikcp_check(left, uv_now(&client_loop)) < ikcp_check(right, uv_now(&client_loop));};
// static std::priority_queue<ikcpcb *, std::vector<ikcpcb *>, decltype(ikcp_cmp)> client_queue(ikcp_cmp);
static const int client_kcp_count = 1;
static ikcpcb *client_kcps[client_kcp_count];
static char client_msg[] = "hello world";
static int client_msg_len = sizeof(client_msg);
static struct sockaddr_in client_addr;
static int client_loop_count = 1; // 1000000
static char client_buf[65536];

// static uv_buf_t client_recv_buf = uv_buf_init(client_msg, client_kcp_count);


static void client_alloc_cb(uv_handle_t* handle,
                            size_t suggested_size,
                            uv_buf_t* buf) {
    assert(suggested_size <= sizeof(client_buf));
    buf->base = client_buf;
    buf->len = sizeof(client_buf);
}

static void client_recv_cb(uv_udp_t* handle,
                           ssize_t nread,
                           const uv_buf_t* rcvbuf,
                           const struct sockaddr* addr,
                           unsigned flags) {
    if (nread < IKCP_OVERHEAD) {
        // log("client_recv_cb"" rcvbuf->len < IKCP_OVERHEAD: %lu\n", rcvbuf->len);
        return;
    }
    IUINT32 conv;
    ikcp_decode32u(rcvbuf->base, &conv);
    log("client_recv_cb conv:%d, len:%ld\n", conv, nread);
    ikcpcb *kcp = client_kcps[conv];
    ikcp_input(kcp, rcvbuf->base, nread);
    int len = ikcp_recv(kcp, client_buf, sizeof(client_msg));
    if (len > 0) {
        log("client_recv_cb success %d\n", len);
        client_loop_count--;
        if (client_loop_count == 0) {
            unsigned long long stoptime = nstime();
            printf("time diff: %llu ns\n", stoptime-starttime);
            exit(0);
            // return;
        };
        log("client ikcp_send len:%d\n", len);
        assert(0 == ikcp_send(kcp, client_buf, len));
    }
}

static void client_udp_send_cb(uv_udp_send_t* req, int status) {
    log("client_udp_send_cb: %d\n", status);
    assert(0 == status);
    free(req);
}

static int client_kcp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    log("client_kcp_output: %d\n", len);
    uv_udp_send_t *req = (uv_udp_send_t *)malloc(sizeof(*req));
    uv_buf_t uv_buf = uv_buf_init((char *)buf, len);
    uv_udp_send(req, &client_udp, &uv_buf, 1, (const struct sockaddr*) &client_addr, client_udp_send_cb);
    return 0;
}

static void kcp_all_update(uv_timer_t *handle) {
    // log("kcp_all_update\n");
    uint64_t now = uv_now(&client_loop);
    // while(!client_queue.empty()) {
    //     ikcpcb *kcp = client_queue.top();
    //     if (ikcp_check(kcp, now) >= now+10) {
    //         return;
    //     }
    //     client_queue.pop();
    //     ikcp_update(kcp, now);
    //     client_queue.push(kcp);
    // }

    for (int i = 0; i < client_kcp_count; ++i)
    {
        ikcp_update(client_kcps[i], now);
    }
}

void client() {
    assert(0 == uv_loop_init(&client_loop));

    assert(0 == uv_udp_init(&client_loop, &client_udp));
    assert(0 == uv_ip4_addr("127.0.0.1", 8000, &client_addr));
    assert(0 == uv_udp_recv_start(&client_udp, client_alloc_cb, client_recv_cb));

    // client_queue

    for (int i = 0; i < client_kcp_count; ++i)
    {
        client_kcps[i] = ikcp_create(i, NULL);
        ikcp_nodelay(client_kcps[i], 1, 10, 2, 1);
        ikcp_setoutput(client_kcps[i], client_kcp_output);
        log("client ikcp_send len: %d\n", client_msg_len);
        // client_queue.push(client_kcps[i]);
    }

    starttime = nstime();
    for (int i = 0; i < client_kcp_count; ++i)
    {
        assert(0 == ikcp_send(client_kcps[i], client_msg, client_msg_len));
    }

    uv_timer_t repeat;
    assert(0 == uv_timer_init(&client_loop, &repeat));
    assert(0 == uv_timer_start(&repeat, kcp_all_update, 10, 10));

    uv_run(&client_loop, UV_RUN_DEFAULT);
}

/////////////////////////////////////////////////////////////////////////
static uv_loop_t server_loop;
static uv_udp_t server_udp;
// static auto ikcp_cmp = [](ikcpcb *left, ikcpcb *right) { return ikcp_check(left, uv_now(&client_loop)) < ikcp_check(right, uv_now(&client_loop));};
// static std::priority_queue<ikcpcb *, std::vector<ikcpcb *>, decltype(ikcp_cmp)> client_queue(ikcp_cmp);
// static const int client_kcp_count = 1;
static ikcpcb *server_kcps[client_kcp_count] = {NULL};
// static char server_msg[client_kcp_count] = {0};
static struct sockaddr_in server_addr;
static struct sockaddr server_addrs[client_kcp_count];
static char server_buf[65536];
// static int client_loop_count = 3;

static void server_alloc_cb(uv_handle_t* handle,
                            size_t suggested_size,
                            uv_buf_t* buf) {
    assert(suggested_size <= sizeof(server_buf));
    buf->base = server_buf;
    buf->len = sizeof(server_buf);
}

static int server_kcp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    log("server_kcp_output: %d\n", len);
    uv_udp_send_t *req = (uv_udp_send_t *)malloc(sizeof(*req)); // 在client_udp_send_cb中free
    uv_buf_t uv_buf = uv_buf_init((char *)buf, len);
    uv_udp_send(req, &server_udp, &uv_buf, 1, (const struct sockaddr*) user, client_udp_send_cb);
    return 0;
}

static void server_recv_cb(uv_udp_t* handle,
                           ssize_t nread,
                           const uv_buf_t* rcvbuf,
                           const struct sockaddr* addr,
                           unsigned flags) {
    if (nread < IKCP_OVERHEAD) {
        // log("server_recv_cb nread < IKCP_OVERHEAD: %d\n", nread);
        return;
    }
    IUINT32 conv;
    ikcp_decode32u(rcvbuf->base, &conv);
    log("server_recv_cb conv:%d, len:%ld\n", conv, nread);
    if (server_kcps[conv] == NULL) {
        server_addrs[conv] = *addr;
        server_kcps[conv] = ikcp_create(conv, &server_addrs[conv]);
        ikcp_nodelay(server_kcps[conv], 1, 10, 2, 1);
        ikcp_setoutput(server_kcps[conv], server_kcp_output);
    }
    ikcpcb *kcp = server_kcps[conv];
    ikcp_input(kcp, rcvbuf->base, nread);
    int len = ikcp_recv(kcp, server_buf, sizeof(client_msg));
    if (len > 0) {
        log("server ikcp_send: len: %d\n", len);
        int ret = ikcp_send(kcp, server_buf, len);
        log("server_recv_cb ikcp_send ret: %d\n", ret);
    }
}

static void server_kcp_all_update(uv_timer_t *handle) {
    // log("kcp_all_update\n");
    uint64_t now = uv_now(&server_loop);
    // while(!client_queue.empty()) {
    //     ikcpcb *kcp = client_queue.top();
    //     if (ikcp_check(kcp, now) >= now+10) {
    //         return;
    //     }
    //     client_queue.pop();
    //     ikcp_update(kcp, now);
    //     client_queue.push(kcp);
    // }

    for (int i = 0; i < client_kcp_count; ++i)
    {
        if (server_kcps[i] != NULL)
            ikcp_update(server_kcps[i], now);
    }
}


void server() {
    assert(0 == uv_loop_init(&server_loop));

    assert(0 == uv_ip4_addr("0.0.0.0", 8000, &server_addr));
    assert(0 == uv_udp_init(&server_loop, &server_udp));
    assert(0 == uv_udp_bind(&server_udp, (const struct sockaddr*) &server_addr, 0));
    assert(0 == uv_udp_recv_start(&server_udp, server_alloc_cb, server_recv_cb));

    uv_timer_t repeat;
    assert(0 == uv_timer_init(&server_loop, &repeat));
    assert(0 == uv_timer_start(&repeat, server_kcp_all_update, 10, 10));

    uv_run(&server_loop, UV_RUN_DEFAULT);
}

/////////////////////////////////////////////////////////////////////////

int main() {
    std::thread server_thread(server);
    std::thread client_thread(client);

    client_thread.join();
    server_thread.join();
}
