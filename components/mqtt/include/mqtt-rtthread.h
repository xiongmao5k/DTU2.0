//
// Created by zhang on 2017/8/9.
//

#ifndef FRAMEWARE_MQTT_RTTHREAD_H
#define FRAMEWARE_MQTT_RTTHREAD_H

#include "stdint.h"
#include "stddef.h"

#include "rtthread.h"

struct __Timer__ {
    rt_tick_t intv;
    rt_tick_t stop;
};
typedef struct __Timer__ Timer;

typedef struct __Network__ Network;
struct __Network__
{
    int sockfd;
    int (*mqttread)(Network*, unsigned char* read_buffer, int, int);
    int (*mqttwrite)(Network*, unsigned char* send_buffer, int, int);
};

rt_err_t rtthread_mqtt_network_connect(Network *n, char *domain, unsigned short port);
Network* rtthread_mqtt_netowrk_create(void);
#endif //FRAMEWARE_MQTT_RTTHREAD_H
