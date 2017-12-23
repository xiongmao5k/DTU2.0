//
// Created by zhang on 2017/8/9.
//

#include "mqtt-rtthread.h"

#include "lwip/netdb.h"

#define MQTT_DEBUG 1

#define RT_TICK_PER_MS (RT_TICK_PER_SECOND / 1000)
// #define TIMER_TICK2MS(tk)   (tk / RT_TICK_PER_MS)
// #define TIMER_MS2TICK(ms)   (ms * RT_TICK_PER_MS)

#define TIMER_TICK2MS(tk)   (tk * 1000 / RT_TICK_PER_SECOND)
#define TIMER_MS2TICK(ms)   (ms * RT_TICK_PER_SECOND / 1000)

void TimerInit(Timer *t) {
    rt_memset(t, 0x00, sizeof(*t));
}

char TimerIsExpired(Timer *t) {
    rt_tick_t now;

    now = rt_tick_get();
    return ((now - t->stop) < (RT_TICK_MAX / 2)) ? 1 : 0;

}

void TimerCountdownMS(Timer *t, unsigned int ms) {
    t->intv = TIMER_MS2TICK(ms);
    t->stop = rt_tick_get() + t->intv;

}

void TimerCountdown(Timer *t, unsigned int sc) {
    t->intv = RT_TICK_PER_SECOND * sc;
    t->stop = rt_tick_get() + t->intv;
}

int TimerLeftMS(Timer *t) {
    rt_tick_t res;

    res = t->stop - rt_tick_get();
    return res < (RT_TICK_MAX / 2) ? TIMER_TICK2MS(res) : 0;
}

int rtthread_mqtt_read(Network *n, unsigned char *rx_buff, int size, int timeout_ms) {
    struct timeval tv;
    int rc;
    int rx_size;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    setsockopt(n->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    rx_size = 0;
    while (rx_size < size) {
        rc = recv(n->sockfd, &rx_buff[rx_size], size - rx_size, 0);
        if (rc < 0) {
            if (errno != ENOTCONN && errno != ECONNRESET) {
                return 0;
            }
        }
        else {
            rx_size += rc;
        }
    }

    return rx_size;
}

int rtthread_mqtt_write(Network *n, unsigned char *tx_buff, int size, int timeout_ms) {
    struct timeval tv;
    int rc;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    setsockopt(n->sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    rc = send(n->sockfd, tx_buff, size, 0);

    if (rc < 0) {
        rc = 0;
    }

    return rc;
}

Network* rtthread_mqtt_netowrk_create(void) {
    Network *res;

    res = rt_malloc(sizeof(*res));
    if (res != RT_NULL) {
        res->sockfd = 0;
        res->mqttwrite = rtthread_mqtt_write;
        res->mqttread = rtthread_mqtt_read;
    }

    return res;
}

rt_err_t rtthread_mqtt_network_connect(Network *n, char *domain, unsigned short port) {
    struct hostent *host;
    struct sockaddr_in addr;
    int soc;
    rt_err_t res;

    host = gethostbyname(domain);
    if (host == RT_NULL) {
        RT_DEBUG_LOG(MQTT_DEBUG, ("%s.%d: get host by name fail.\n", __FUNCTION__, __LINE__));
        return RT_ERROR;
    }

    soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc < 0) {
        RT_DEBUG_LOG(MQTT_DEBUG, ("%s.%d: create sock fail.\n", __FUNCTION__, __LINE__));
        return RT_EIO;
    }

    rt_memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = *((struct in_addr *)host->h_addr);
    addr.sin_port = htons(port);
    res = connect(soc, (struct sockaddr *)&addr, sizeof(addr));
    if (res < 0) {
        RT_DEBUG_LOG(MQTT_DEBUG, ("%s.%d: connect sock fail.\n", __FUNCTION__, __LINE__));
        lwip_close(soc);
        return EIO;
    }

    n->sockfd = soc;

    return RT_EOK;
}
