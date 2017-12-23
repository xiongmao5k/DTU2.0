//
// Created by zhang on 2017/8/7.
//

#ifndef FRAMEWARE_MQTTCLIENT_H
#define FRAMEWARE_MQTTCLIENT_H

#include "rtthread.h"

typedef struct mqtt_client mqtt_client_t;
struct mqtt_client {
    mqtt_client_t *next;

    unsigned char *rx_buff;
    unsigned char *tx_buff;
    unsigned int rx_buff_size;
    unsigned int tx_buff_size;

    int socket_fd;

    rt_tick_t keeplive;
    rt_tick_t keeplive_stop;

    rt_mutex_t mutex_socket;
};




#endif //FRAMEWARE_MQTTCLIENT_H
