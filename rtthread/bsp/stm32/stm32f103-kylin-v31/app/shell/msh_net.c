#include <rtthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include "app_config.h"
#include "app_types.h"
#include "svc_net.h"

static int cmd_net(int argc, char **argv)
{
    if (argc == 1)
    {
        rt_kprintf("net link: %s\n", svc_net_is_ready() ? "up" : "down");
        rt_kprintf("usage: net tcp <host> <port>\n");
        return 0;
    }

    if (rt_strcmp(argv[1], "tcp") == 0)
    {
        struct hostent *host;
        struct sockaddr_in addr;
        int sock;
        int port;
        char buf[513];
        int len;

        if (argc != 4)
        {
            rt_kprintf("usage: net tcp <host> <port>\n");
            return -1;
        }

        port = atoi(argv[3]);
        if ((port <= 0) || (port > 65535))
        {
            rt_kprintf("net: invalid port\n");
            return -1;
        }

        if (!svc_net_is_ready())
        {
            rt_kprintf("net: link down\n");
            return -1;
        }

        host = gethostbyname(argv[2]);
        if (host == RT_NULL)
        {
            rt_kprintf("net: resolve %s failed\n", argv[2]);
            return -1;
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            rt_kprintf("net: socket failed\n");
            return -1;
        }

        rt_memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((rt_uint16_t)port);
        addr.sin_addr = *((struct in_addr *)host->h_addr);

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            rt_kprintf("net: connect %s:%d failed\n", argv[2], port);
            closesocket(sock);
            return -1;
        }

        rt_kprintf("net: connected %s:%d\n", argv[2], port);

        len = sal_sendto(sock, "hello from kylin\r\n", 18, 0, RT_NULL, 0);
        rt_kprintf("net: send %d bytes\n", len);

        len = sal_recvfrom(sock, buf, sizeof(buf) - 1, 0, RT_NULL, RT_NULL);
        if (len > 0)
        {
            buf[len] = '\0';
            rt_kprintf("net: recv %d bytes: %s\n", len, buf);
        }
        else
        {
            rt_kprintf("net: recv %d\n", len);
        }

        closesocket(sock);
        return 0;
    }

    rt_kprintf("usage: net tcp <host> <port>\n");
    return -1;
}
MSH_CMD_EXPORT_ALIAS(cmd_net, net, network status and TCP test);
