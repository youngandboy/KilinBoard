#include <rtthread.h>
#include <rtdevice.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <at_device_esp8266.h>
#include <netdev.h>
#include "app_config.h"
#include "app_types.h"
#include "svc_storage.h"
#include "svc_net.h"

static struct at_device_esp8266 esp0;
static int net_registered;
static int net_socket = -1;
static int listen_socket = -1;

void svc_net_reset(void)
{
    rt_pin_mode(APP_ESP8266_RESET_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(APP_ESP8266_RESET_PIN, PIN_LOW);
    rt_thread_mdelay(100);
    rt_pin_write(APP_ESP8266_RESET_PIN, PIN_HIGH);
    rt_thread_mdelay(1500);
}

int svc_net_init(void)
{
    const app_cfg_t *cfg = svc_storage_get();

    if (cfg->wifi_ssid[0] == '\0')
    {
        rt_kprintf("[net] no wifi config, skip at_device init\n");
        return APP_OK;
    }

    svc_net_reset();

    esp0.device_name   = "esp0";
    esp0.client_name   = APP_ESP8266_UART;
    esp0.wifi_ssid     = (char *)cfg->wifi_ssid;
    esp0.wifi_password = (char *)cfg->wifi_pwd;
    esp0.recv_line_num = 512;
    esp0.user_data     = RT_NULL;

    if (at_device_register(&esp0.device,
                           "esp0",
                           APP_ESP8266_UART,
                           AT_DEVICE_CLASS_ESP8266,
                           &esp0) != RT_EOK)
    {
        rt_kprintf("[net] at_device_register failed\n");
        return APP_ERR;
    }

    net_registered = 1;
    rt_kprintf("[net] esp0 registered, ssid=%s\n", cfg->wifi_ssid);

    return APP_OK;
}

int svc_net_is_ready(void)
{
    struct netdev *dev;

    if (!net_registered)
    {
        return 0;
    }

    dev = netdev_get_by_name("esp0");
    if (dev == RT_NULL)
    {
        return 0;
    }

    return netdev_is_link_up(dev);
}

int svc_net_socket_connect(const char *host, rt_uint16_t port)
{    struct hostent *host_info;
    struct sockaddr_in addr;
    struct timeval tv;
    int sock;

    if ((host == RT_NULL) || (port == 0))
    {
        return APP_ERR_PARAM;
    }

    svc_net_socket_close();

    host_info = gethostbyname(host);
    if (host_info == RT_NULL)
    {
        rt_kprintf("[net] resolve %s failed\n", host);
        return APP_ERR_IO;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        rt_kprintf("[net] socket failed\n");
        return APP_ERR_IO;
    }

    rt_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = *((struct in_addr *)host_info->h_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        rt_kprintf("[net] connect %s:%u failed\n", host, port);
        closesocket(sock);
        return APP_ERR_IO;
    }

    /* Set timeout after connect; AT socket init resets it. */
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    net_socket = sock;
    rt_kprintf("[net] connected %s:%u\n", host, port);
    return APP_OK;
}

int svc_net_server_start(rt_uint16_t port)
{
    struct sockaddr_in addr;
    int sock;

    if (port == 0)
    {
        return APP_ERR_PARAM;
    }
    if (listen_socket >= 0)
    {
        return APP_OK;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        rt_kprintf("[net] server socket failed\n");
        return APP_ERR_IO;
    }

    rt_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        rt_kprintf("[net] bind %u failed\n", port);
        closesocket(sock);
        return APP_ERR_IO;
    }

    if (listen(sock, 1) < 0)
    {
        rt_kprintf("[net] listen %u failed\n", port);
        closesocket(sock);
        return APP_ERR_IO;
    }

    listen_socket = sock;
    rt_kprintf("[net] server listening on port %u\n", port);
    return APP_OK;
}

int svc_net_server_accept(void)
{
    struct timeval tv;
    int sock;

    if (listen_socket < 0)
    {
        return APP_ERR_STATE;
    }

    sock = accept(listen_socket, RT_NULL, RT_NULL);
    if (sock < 0)
    {
        return APP_ERR_IO;
    }

    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    net_socket = sock;
    rt_kprintf("[net] web collector connected\n");
    return APP_OK;
}

int svc_net_socket_send(const void *buf, rt_size_t len)
{
    int sent;

    if ((net_socket < 0) || (buf == RT_NULL) || (len == 0))
    {
        return APP_ERR_STATE;
    }

    sent = sal_sendto(net_socket, buf, len, 0, RT_NULL, 0);
    if (sent < 0)
    {
        rt_kprintf("[net] send failed\n");
        svc_net_socket_close();
        return APP_ERR_IO;
    }

    return sent;
}

int svc_net_socket_recv(void *buf, rt_size_t len)
{
    int received;

    if ((net_socket < 0) || (buf == RT_NULL) || (len == 0))
    {
        return APP_ERR_STATE;
    }

    received = sal_recvfrom(net_socket, buf, len, MSG_DONTWAIT, RT_NULL, RT_NULL);
    if (received < 0)
    {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        {
            return 0;
        }
        svc_net_socket_close();
        return APP_ERR_IO;
    }

    return received;
}

void svc_net_socket_close(void)
{
    if (net_socket >= 0)
    {
        closesocket(net_socket);
        net_socket = -1;
    }
}

int svc_net_socket_is_connected(void)
{
    return net_socket >= 0;
}
