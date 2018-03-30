// g++ kcpp_test.cpp ikcp.c -o kcpp_test -I. -Ideps/libuv/include -Ldeps/libuv/libs -luv

#include "ikcp.h"
// #include <uv.h>

const int CONV = 0x01

ikcpcb *kcp1 = NULL;
ikcpcb *kcp2 = NULL;

int count = 20;

// 底层发送函数
static int on_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    ikcpcb *dst_kcp = (kcp == kcp1) ? kcp2 : kcp1;
    if (--count == 0) {
        exit(0);
    }
    return ikcp_input(dst_kcp, buf, len);
}

// 上层接收函数
static int on_recv(const char *buf, int len, ikcpcb *kcp, void *user)
{
    ikcpcb *dst_kcp = (kcp == kcp1) ? kcp2 : kcp1;
    return ikcp_send(dst_kcp, buf, len);
}

int main() {
    kcp1 = ikcp_create(CONV, NULL);
    ikcp_setoutput(kcp1, on_output);
    ikcp_setrecv(kcp1, on_recv);

    kcp2 = ikcp_create(CONV, NULL);
    ikcp_setoutput(kcp2, on_output);
    ikcp_setrecv(kcp2, on_recv);

    ickp_send(kcp1, "123", 3);

    ikcp_release(kcp1);
    ikcp_release(kcp2);

    return 0;
}

