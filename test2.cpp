// g++ kcpp_test.cpp -o kcpp_test -I. -Ideps/libuv/include -Ldeps/libuv/libs -luv
#include "test.h"
#include "ikcp.c"
// #include <uv.h>

const int CONV = 0x01;
char msg[] = "123";
const int MSG_LEN = 3;

ikcpcb *kcp1 = NULL;
ikcpcb *kcp2 = NULL;

int count = 1000;

// 底层发送函数
static int on_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    ikcpcb *dst_kcp = (kcp == kcp1) ? kcp2 : kcp1;
    if (kcp == kcp1 && --count == 0) {
        printf("success\n");
        exit(0);
    }
    return ikcp_input(dst_kcp, buf, len);
}

// 上层接收函数
static void on_recv(const char *buf, int len, ikcpcb *kcp, void *user)
{
    if (len != MSG_LEN) {
        printf("on_recv len invalid: %d\n", len);
    }
    // ikcpcb *dst_kcp = (kcp == kcp1) ? kcp2 : kcp1;
    int ret = ikcp_send(kcp, buf, len);
    if (ret != MSG_LEN) {
        printf("on_recv ikcp_send error: %d, count: %d\n", ret, count);
    }
}

int main() {
    kcp1 = ikcp_create(CONV, NULL);
    ikcp_nodelay(kcp1, 1, 10, 2, 1);
    ikcp_setoutput(kcp1, on_output);
    ikcp_setrecv(kcp1, on_recv);

    kcp2 = ikcp_create(CONV, NULL);
    ikcp_nodelay(kcp2, 1, 10, 2, 1);
    ikcp_setoutput(kcp2, on_output);
    ikcp_setrecv(kcp2, on_recv);

    ikcp_send(kcp1, msg, MSG_LEN);

    ikcp_release(kcp1);
    ikcp_release(kcp2);

    return 0;
}


