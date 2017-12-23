//
// Created by zhang on 2017/8/3.
//

#include "rtthread.h"

#include "ppp.h"

#define APP_DEBUG 1

#if 0
#include "MQTTClient.h"

#define MQTT_TEST_RX_BUFF_SIZE  1024
#define MQTT_TEST_TX_BUFF_SIZE  1024

void mqtt_test_entry(void) {
    Network *n;
    MQTTClient client;
    rt_err_t res;
    char *rx_buf, *tx_buf;
    MQTTPacket_connectData conn_data = MQTTPacket_connectData_initializer;

    RT_DEBUG_LOG(APP_DEBUG, ("%s.%d: mqtt test start.\n", __FUNCTION__, __LINE__));

    rx_buf = rt_malloc(MQTT_TEST_RX_BUFF_SIZE);
    if (rx_buf == RT_NULL) {
        return;
    }

    tx_buf = rt_malloc(MQTT_TEST_TX_BUFF_SIZE);
    if (tx_buf == RT_NULL) {
        rt_free(rx_buf);
        return;
    }

    n = rtthread_mqtt_netowrk_create();
    if (n == RT_NULL) {
        RT_DEBUG_LOG(APP_DEBUG, ("%s.%d: create mqtt network fail.\n", __FUNCTION__, __LINE__));
        goto EXIT;
    }

    res = rtthread_mqtt_network_connect(n, "122.224.168.138", 41001);
    if (res != RT_EOK) {
        RT_DEBUG_LOG(APP_DEBUG, ("%s.%d: connect network fail.\n", __FUNCTION__, __LINE__));
        goto EXIT;
    }

    MQTTClientInit(&client, n, 30000,
                   tx_buf, MQTT_TEST_TX_BUFF_SIZE,
                   rx_buf, MQTT_TEST_RX_BUFF_SIZE);

    RT_DEBUG_LOG(APP_DEBUG, ("%s.%d: connect mqtt server.\n", __FUNCTION__, __LINE__));

    conn_data.willFlag = 0;
    conn_data.MQTTVersion = 3;
    conn_data.clientID.cstring = "mqtt_test_entry";
    conn_data.username.cstring = "";
    conn_data.password.cstring = "";
    conn_data.keepAliveInterval = 70;
    conn_data.cleansession = 1;

    res = MQTTConnect(&client, &conn_data);
    if (res != SUCCESS) {
        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: mqtt connect fail(res = %d).\n", __FUNCTION__, __LINE__, res));
        goto EXIT;
    }

    void message_arrived(MessageData *data) {
        rt_kprintf("Message arrived on topic %.*s: %.*s\n",
                   data->topicName->lenstring.len,
                   data->topicName->lenstring.data,
                   data->message->payloadlen,
                   data->message->payload);
    }
    res = MQTTSubscribe(&client, "mqtt_test", QOS0, message_arrived);
    if (res != SUCCESS) {
        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: mqtt subscribe fail(res = %d).\n", __FUNCTION__, __LINE__, res));
        goto EXIT;
    }

    while (1) {
        int res;

        res = MQTTYield(&client, 35000);
        if (!(res == RT_EOK)) {
            RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: mqtt yield fail(res = %d).\n", __FUNCTION__, __LINE__, res));
            goto EXIT;
        }
        rt_thread_delay(RT_TICK_PER_SECOND * 0.1);
    }
    EXIT:
    rt_free(rx_buf);
    rt_free(tx_buf);
}

extern void lwip_system_init(void);

rt_err_t gsm_start(void) {
    rt_device_t gsm_dev;
    rt_err_t res;
    char *gsm_dial_cmd = "ATDT*99***1#\r\n";
    int tsf_size;
    char rx_buff[64];

    gsm_dev = rt_device_find("serial2");
    if (gsm_dev == RT_NULL) {
        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: %s", __FUNCTION__, __LINE__, "find gsm_dev fail.\n"));
        return RT_EIO;
    }

    res = rt_device_open(gsm_dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_OFLAG_RDWR);
    if (res > 0) {
        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: %s", __FUNCTION__, __LINE__, "open gsm_dev fail.\n"));
        return res;
    }

    tsf_size = rt_device_write(gsm_dev, 0, gsm_dial_cmd, rt_strlen(gsm_dial_cmd));
    if (tsf_size != rt_strlen(gsm_dial_cmd)) {
        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: %s", __FUNCTION__, __LINE__, "write gsm_dev fail.\n"));
        return RT_EIO;
    }

    rt_thread_delay(RT_TICK_PER_SECOND * 8);
    rt_memset(rx_buff, 0x00, sizeof(rx_buff));
    while (1) {
        tsf_size = rt_device_read(gsm_dev, 0, rx_buff, sizeof(rx_buff));
        if (tsf_size < 0) {
            return RT_EIO;
        } else if (tsf_size == 0) {
            rt_thread_delay(RT_TICK_PER_SECOND * 1);
            continue;
        }
        break;
    }

    if (rt_strstr(rx_buff, "CONN") != RT_NULL) {
        void ppp_link_status_cb(void *ctx, int errCode, void *arg) {
            switch (errCode) {
                case PPPERR_NONE: {
                    rt_thread_t mqtt_thread;

                    RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: %s", __FUNCTION__, __LINE__, "ppp connect success.\n"));

                    mqtt_thread = rt_thread_create("mqtt_thread", mqtt_test_entry, RT_NULL, 4096, 20, 10);

                    if (mqtt_thread != RT_NULL) {
                        rt_thread_startup(mqtt_thread);
                    } else {
                        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: %s", __FUNCTION__, __LINE__, "create mqtt test thread fail.\n"));
                    }

                    break;
                }
                case PPPERR_PARAM:      /* Invalid parameter. */
                case PPPERR_OPEN:       /* Unable to open PPP session. */
                case PPPERR_DEVICE:     /* Invalid I/O device for PPP. */
                case PPPERR_ALLOC:      /* Unable to allocate resources. */
                case PPPERR_USER:       /* User interrupt. */
                case PPPERR_CONNECT:    /* Connection lost. */
                case PPPERR_AUTHFAIL:   /* Failed authentication challenge. */
                case PPPERR_PROTOCOL:   /* Failed to meet protocol. */
                default:
                    break;
            }
        }

        lwip_system_init();

        pppInit();
        pppSetAuth(PPPAUTHTYPE_ANY, "", "");

        pppOverSerialOpen(gsm_dev, ppp_link_status_cb, RT_NULL);
    } else {
        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: %s", __FUNCTION__, __LINE__, "GSM dial fail.\n"));
        RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: GSM dial recv %s", __FUNCTION__, __LINE__, rx_buff));
    }

}

#endif

void user_application_init(void) {
    RT_DEBUG_LOG(APP_DEBUG, ("[%s.%d]: %s", __FUNCTION__, __LINE__, "user application start.\n"));
    // gsm_start();
}
