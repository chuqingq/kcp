// g++ kcpp_test.cpp -o kcpp_test -I. -Ideps/libuv/include -Ldeps/libuv/libs -luv
#include "test.h"
#include "ikcp.c"
// #include <uv.h>

const int CONV = 0x01;

ikcpcb *kcp1 = NULL;
ikcpcb *kcp2 = NULL;

int count = 3;

// 底层发送函数
static int on_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
    printf("output\n");
    ikcpcb *dst_kcp = (kcp == kcp1) ? kcp2 : kcp1;
    if (kcp == kcp1 && --count == 0) {
        printf("end\n");
        exit(0);
    }
    return ikcp_input(dst_kcp, buf, len);
}

// 上层接收函数
static void on_recv(const char *buf, int len, ikcpcb *kcp, void *user)
{
    ikcpcb *dst_kcp = (kcp == kcp1) ? kcp2 : kcp1;
    ikcp_send(dst_kcp, buf, len);
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

    ikcp_send(kcp1, "123", 3);

    ikcp_release(kcp1);
    ikcp_release(kcp2);

    return 0;
}


