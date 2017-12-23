//
// Created by zhang on 2017/8/7.
//

#include <MQTTConnect.h>
#include <MQTTPacket.h>
#include "mqttclient.h"

#include "lwip/netdb.h"
#include "arch/sys_arch.h"

#include "MQTTPacket.h"

#define MQTT_WRITE_TIMEOUT 5
#define MQTT_READ_TIMEOUT 5

int mqtt_write(mqtt_client_t *client, unsigned char *buf,  int len)
{
    int rc;
    struct timeval tv;

    tv.tv_sec = MQTT_WRITE_TIMEOUT;
    tv.tv_usec = 0;

    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
    rc = send(client->socket_fd, buf, len, 0);
    if (rc == len)
        rc = 0;

    return rc;
}

static int mqtt_read(mqtt_client_t *client, void *buff, unsigned int rx_len) {
    char *base = buff;
    int bytes = 0;
    struct timeval interval;
    int rc;

    interval.tv_sec = MQTT_READ_TIMEOUT;
    interval.tv_usec = 0;

    rc = setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&interval, sizeof(struct timeval));

    while (bytes < rx_len)
    {
        rc = recv(client->socket_fd, &base[bytes], (size_t)(rx_len - bytes), 0);

        if (rc == -1)
        {
            if (errno != ENOTCONN && errno != ECONNRESET)
            {
                bytes = -1;
                break;
            }
        }
        else
            bytes += rc;
    }

    return bytes;
}

static void mqtt_client_poll(mqtt_client_t *client) {
    int pkgt;
    rt_err_t err;

    err = rt_mutex_take(client->mutex_socket, 0);
    if (err != RT_EOK) {
        return;
    }

    /* recv publish message */
    static int __packet_read(void *buff, unsigned int len) {
        return mqtt_read(client, buff, len);
    }
    pkgt = MQTTPacket_read(client->rx_buff, client->rx_buff_size, __packet_read);
    if (pkgt = PUBLISH) {
        /* do something */

        { /* refurbish keeplive timer */
            rt_tick_t now = rt_tick_get();
            rt_tick_t stop = client->keeplive_stop;

            if ((now - stop) > client->keeplive) {
                /* send ping pack */
            }
        }
    }

    rt_mutex_release(client->mutex_socket);


    /* check keeplive */
    // switch (pkgt) {
    //     case CONNACK:
    //     case PUBACK:
    //     case SUBACK:
    //         break;
    //     case PUBLISH: {
    //         break;
    //     }
    //     case PUBCOMP:
    //     case PINGRESP:
    //         break;
    //     default:
    //         break;
    // }
}

struct mqtt_client_head {
    mqtt_client_t *first;
    rt_mutex_t mutex_client_head;
};
struct mqtt_client_head mqtt_head;

static void mqtt_thread_entry(void *arg) {
    mqtt_client_t *client;

    while (1) {
        rt_err_t res;

        res = rt_mutex_take(mqtt_head.mutex_client_head, 0);
        if (res == RT_EOK) {
            for (client = mqtt_head.first; client != NULL; client = client->next) {
                mqtt_client_poll(client);
            }
            rt_mutex_release(mqtt_head.mutex_client_head);
        }
        rt_thread_delay(RT_TICK_PER_SECOND * 0.1);
    }
}

void mqtt_init(void) {
    mqtt_head.first = RT_NULL;
    mqtt_head.mutex_client_head = rt_mutex_create("mqtt head mutex", RT_IPC_FLAG_FIFO);
}

typedef MQTTPacket_connectData mqtt_conn_info_t;

rt_err_t mqtt_connect(mqtt_client_t *client,
                              const char *domain, unsigned short port,
                              mqtt_conn_info_t *info) {

    struct hostent *host;
    struct sockaddr_in addr;
    int soc;
    rt_err_t res;

    host = gethostbyname(domain);
    if (host == RT_NULL) {
        return RT_ERROR;
    }

    soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc < 0) {
        return RT_EIO;
    }

    rt_memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = *((struct in_addr *)host->h_addr_list);
    addr.sin_port = htons(port);
    res = connect(soc, (struct sockaddr *)&addr, sizeof(addr));
    if (res < 0) {
        lwip_close(soc);
        return EIO;
    }

    int len;

    len = MQTTSerialize_connect(client->tx_buff, client->tx_buff_size, info);
    if (len < 0) {
        goto UNLUCK_END;
    }
    res = mqtt_write(client, client->tx_buff, len);
    if (res < 0) {
        goto UNLUCK_END;
    }

    int __packet_read(void *buff, unsigned int len) {
        return mqtt_read(client, buff, len);
    }
    res = MQTTPacket_read(client->rx_buff, client->rx_buff_size, __packet_read);
    if (res < 0) {
        goto UNLUCK_END;
    }
    if (res == CONNACK) {
        unsigned char session_persion, connack_rc;
        if (MQTTDeserialize_connack(&session_persion, &connack_rc, client->rx_buff, len) == 1) {
            if (connack_rc != 0) {
                goto UNLUCK_END;
            }
            if (session_persion == 1) {
                /* cl */
            }

            res = rt_mutex_take(mqtt_head.mutex_client_head, RT_TICK_PER_SECOND * 5);
            if (res == RT_EOK) {
                client->next = mqtt_head.first;
                mqtt_head.first = client;
                rt_mutex_release(mqtt_head.mutex_client_head);
            }
            else {
                lwip_close(client->socket_fd);
                return res;
            }
        }
        else {
            goto UNLUCK_END;
        }
    }
    else {
        goto UNLUCK_END;
    }

    return RT_EOK;

UNLUCK_END:
    lwip_close(soc);

    return RT_EIO;
}

rt_err_t mqtt_disconnect(mqtt_client_t *client) {
    int len;
    int rc;

    len = MQTTSerialize_disconnect(client->tx_buff, client->tx_buff_size);
    if (len > 0) {
        rc = mqtt_write(client, client->tx_buff, len);
    }
    return rc;
}

rt_err_t mqtt_subscribe(mqtt_client_t *client, char *topicstr, int qos) {
    MQTTString topic = MQTTString_initializer;
    int msg_id = 1;
    int len;
    int rc = -1;
    int res;

    topic.cstring = topicstr;
    len = MQTTSerialize_subscribe(client->tx_buff, client->tx_buff_size, 0, msg_id, 1, &topic, &qos);
    if (len < 0) {
        return RT_ERROR;
    }

    rc = mqtt_write(client, client->tx_buff, len);
    if (rc < 0) {
        return RT_EIO;
    }

    int __packet_read(void *buff, unsigned int len) {
        return mqtt_read(client, buff, len);
    }
    res = MQTTPacket_read(client->rx_buff, client->rx_buff_size, __packet_read);
    if (res == SUBACK) {
        unsigned short submsg_id;
        int sub_count;
        int granted_qos;

        rc = MQTTDeserialize_suback(&submsg_id, 1, &sub_count, &granted_qos, client->rx_buff, client->rx_buff_size);
        if (granted_qos != 0) {
            return RT_ERROR;
        }
        else {
            return RT_EOK;
        }
    }
    else {
        return RT_ERROR;
    }
}

struct mqtt_msg {
    int qos;
    unsigned char retained;
    unsigned char dup;
    unsigned short id;
    int payloadlen;
    unsigned char *payload;
};
typedef struct mqtt_msg mqtt_msg_t;

rt_err_t mqtt_publish(mqtt_client_t *client, char *topicstr, mqtt_msg_t *msg) {
    // unsigned char* buf, int buflen, unsigned char dup, int qos, unsigned char retained, unsigned short packetid,
    // MQTTString topicName, unsigned char* payload, int payloadlen)
    MQTTString topic = MQTTString_initializer;
    int len;
    int rc;

    topic.cstring = topicstr;

    len = MQTTSerialize_publish(client->tx_buff, client->tx_buff_size,
                          msg->dup, msg->qos, msg->retained,
                          msg->id,
                          topic, msg->payload, msg->payloadlen);
    if (len < 0) {
        return RT_ERROR;
    }
    rc = mqtt_write(client, client->tx_buff, len);
    if (rc < 0) {
        return RT_EIO;
    }

    int pkgt;
    int __packet_read(void *buff, unsigned int len) {
        return mqtt_read(client, buff, len);
    }
    pkgt = MQTTPacket_read(client->rx_buff, client->rx_buff_size, __packet_read);
    switch (msg->qos) {
        case 1: {

            break;
        }
        case 2: {
            break;
        }
        default:
            break;
    }


}

