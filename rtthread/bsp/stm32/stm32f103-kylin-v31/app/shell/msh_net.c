#include <rtthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include "app_config.h"
#include "app_types.h"
#include "svc_net.h"
#include "task_net.h"

static int cmd_net(int argc, char **argv)
{
    if (argc == 1)
    {
        rt_kprintf("net link: %s, socket: %s\n",
                   svc_net_is_ready() ? "up" : "down",
                   svc_net_socket_is_connected() ? "connected" : "closed");
        rt_kprintf("usage: net stat | net tcp <host> <port>\n");
        return 0;
    }

    if (rt_strcmp(argv[1], "stat") == 0)
    {
        app_net_stats_t s;
        rt_tick_t now = rt_tick_get();
        rt_uint32_t tx_age = 0;

        task_net_get_stats(&s);
        if (s.last_tx_tick != 0)
        {
            tx_age = (rt_uint32_t)((now - s.last_tx_tick) / RT_TICK_PER_SECOND);
        }

        rt_kprintf("net stat: link=%s socket=%s backoff=%u ms last_error=%d\n",
                   svc_net_is_ready() ? "up" : "down",
                   svc_net_socket_is_connected() ? "connected" : "closed",
                   s.backoff_ms, s.last_error);
        rt_kprintf("  connect: attempts=%u ok=%u disconnects=%u\n",
                   s.connect_attempts, s.connect_ok, s.disconnects);
        rt_kprintf("  tx: frames=%u errors=%u dropped=%u last_tx=%us\n",
                   s.tx_frames, s.tx_errors, s.tx_dropped, tx_age);
        rt_kprintf("  rx: packets=%u commands=%u errors=%u\n",
                   s.rx_packets, s.rx_commands, s.rx_errors);
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

    rt_kprintf("usage: net stat | net tcp <host> <port>\n");
    return -1;
}
MSH_CMD_EXPORT_ALIAS(cmd_net, net, network status and TCP test);
