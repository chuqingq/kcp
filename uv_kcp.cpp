// libuv+kcp
// g++ uv_kcp.cpp -g -std=c++11 -pthread -I../libuv/libuv_bin/include -L../libuv/libuv_bin/lib -luv
// LD_LIBRARY_PATH=../libuv/libuv_bin/lib ./a.out
#include <thread>
#include <cassert>
#include <queue>
#include <vector>
 
#include <uv.h>
#include "ikcp.c"

/////////////////////////////////////////////////////////////////////////////////

static uv_loop_t client_loop;
static uv_udp_t client_udp;
static auto ikcp_cmp = [](ikcpcb *left, ikcpcb *right) { return ikcp_check(left, uv_now(&client_loop)) < ikcp_check(right, uv_now(&client_loop));};
static std::priority_queue<ikcpcb *, std::vector<ikcpcb *>, decltype(ikcp_cmp)> client_queue(ikcp_cmp);
static const int client_kcp_count = 1;
static ikcpcb *client_kcps[client_kcp_count];
static char client_msg[client_kcp_count] = {0};
static struct sockaddr_in server_addr;
static int client_loop_count = 3;
// static uv_buf_t client_recv_buf = uv_buf_init(client_msg, client_kcp_count);


static void client_alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
    assert(suggested_size <= client_kcp_count);
    buf->base = client_msg;
    buf->len = suggested_size;
}

static void client_recv_cb(uv_udp_t* handle,
                       ssize_t nread,
                       const uv_buf_t* rcvbuf,
                       const struct sockaddr* addr,
                       unsigned flags) {
    if (rcvbuf->len < IKCP_OVERHEAD) {
        printf("client_recv_cb"" rcvbuf->len < IKCP_OVERHEAD: %lu\n", rcvbuf->len);
        return;
    }
    IUINT32 conv;
    ikcp_decode32u(rcvbuf->base, &conv);
    ikcpcb *kcp = client_kcps[conv];
    ikcp_input(kcp, rcvbuf->base, rcvbuf->len);
    if (ikcp_recv(kcp, client_msg, client_kcp_count) == 0) {
    	client_loop_count--;
    	if (client_loop_count == 0) {
    		printf("success\n");
    	} else {
    		printf("1\n");
    		assert(0 == ikcp_send(kcp, client_msg, client_kcp_count));
    	}
    }
}

static void client_udp_send_cb(uv_udp_send_t* req, int status) {
	printf("send_cb: %d\n", status);
	assert(0 == status);
	free(req);
}

static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
	printf("kcp_output\n");
    uv_udp_send_t *req = (uv_udp_send_t *)malloc(sizeof(*req));
    uv_buf_t uv_buf = uv_buf_init((char *)buf, len);
    uv_udp_send(req, &client_udp, &uv_buf, 1, (const struct sockaddr*) &server_addr, client_udp_send_cb);
}

static void kcp_all_update(uv_timer_t *handle) {
	// printf("kcp_all_update\n");
    uint64_t now = uv_now(&client_loop);
    // while(!client_queue.empty()) {
    //     ikcpcb *kcp = client_queue.top();
    //     if (ikcp_check(kcp, now) >= now+10) {
    //     	return;
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

    assert(0 == uv_udp_recv_start(&client_udp, client_alloc_cb, client_recv_cb));
    assert(0 == uv_ip4_addr("127.0.0.1", 1234, &server_addr));

    // client_queue

    for (int i = 0; i < client_kcp_count; ++i)
    {
        client_kcps[i] = ikcp_create(i, NULL);
        ikcp_setoutput(client_kcps[i], kcp_output);
        assert(0 == ikcp_send(client_kcps[i], client_msg, i));

        client_queue.push(client_kcps[i]);
    }

    uv_timer_t repeat;
    assert(0 == uv_timer_init(&client_loop, &repeat));
    assert(0 == uv_timer_start(&repeat, kcp_all_update, 10, 10));

    uv_run(&client_loop, UV_RUN_DEFAULT);
}

/////////////////////////////////////////////////////////////////////////
void server() {

}

/////////////////////////////////////////////////////////////////////////

int main() {
    std::thread server_thread(server);
    std::thread client_thread(client);

    client_thread.join();
    server_thread.join();
}