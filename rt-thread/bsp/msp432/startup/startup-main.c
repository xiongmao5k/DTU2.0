#include <stdint.h>
#include <stddef.h>

#include "driverlib.h"
#include "board.h"

#include "rthw.h"
#include "rtthread.h"
#ifdef RT_USING_FINSH
#include "shell.h"
#endif

#include "ppp.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

// extern char __heap_start__;
unsigned int __heap_start__;
// extern int Image$$RW_IRAM1$$ZI$$Limit;
// #pragma section="HEAP"

typedef struct
{
    int qos;
    unsigned char retained;
    unsigned char dup;
    unsigned short id;
    int payloadlen;
    unsigned char *payload;
} mqtt_msg_t;

typedef struct
{
    int sockfd;
    unsigned char *wbuf; //
    int wbuflen;
    unsigned char *rbuf;
    int rbuflen;
    int (*getfn)(unsigned char*, int);
} mqtt_client_t;

void mqtt_test_thread_entry(void *param) {
#define HOSTNAME     "m2m.eclipse.org"
#define HOSTPORT     1883
#define USERNAME     "testuser"
#define PASSWORD     "testpassword"
#define TOPIC        "test"

#define KEEPALIVE_INTERVAL    20


}

void network_test_thread_entry(void *param) {
    struct hostent *host;
    struct sockaddr_in se_addr;
    int soc, res;

    host = gethostbyname("www.baidu.com");
    if (host != RT_NULL) {
        rt_kprintf("get host by name success.\n");
        if (host->h_length > 0) {
            rt_kprintf("host ip address: [%s].\n", ip_ntoa((ip_addr_t *)host->h_addr));
            rt_kprintf("host ip address: [%s].\n", inet_ntoa(*host->h_addr));
        }
    }
    else {
        rt_kprintf("get host by name fail.\n");
    }

    if (1) {
        char *rx_buff;

        rx_buff = rt_malloc(1024);
        if (rx_buff == RT_NULL) {
            rt_kprintf("no memory\n");
            return;
        }

        START_CONNECT:
        soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (soc == -1) {
            rt_kprintf("socket error\n.");
            rt_free(rx_buff);
            return;
        }

        rt_memset(&se_addr, 0x00, sizeof(se_addr));
        se_addr.sin_family = AF_INET;
        se_addr.sin_port = htons(41001);
        // se_addr.sin_port = htons(50000);
        se_addr.sin_addr.s_addr = inet_addr("122.224.168.138");
        // se_addr.sin_port = htons(80);
        // se_addr.sin_addr.s_addr = inet_addr("115.239.210.27");

        res = connect(soc, (struct sockaddr *)&se_addr, sizeof(struct sockaddr));
        if (res == -1) {
            rt_kprintf("connect 122.224.168.138 fail.\n");
            rt_thread_delay(RT_TICK_PER_SECOND * 2);
            rt_kprintf("reconnect 122.224.168.138.\n");
            lwip_close(soc);
            goto START_CONNECT;
            // rt_free(rx_buff);
            // return;
        }
        else {
            rt_kprintf("TCP connect success.\n");
        }

        while (1) {
            int rx_size;

            rx_size = recv(soc, rx_buff, 1024, 0);
            if (rx_size < 0) {
                lwip_close(soc);
                rt_thread_delay(RT_TICK_PER_SECOND * 2);
                goto START_CONNECT;
            }
            else {
                if (rx_size > 0) {
                    send(soc, rx_buff, rx_size, 0);
                }
            }

            rt_thread_delay(RT_TICK_PER_SECOND * 0.5);

            // char *tx_msg = "Hello tcpip.\r\n";
            // send(soc, tx_msg, rt_strlen(tx_msg), 0);

            // rt_thread_delay(RT_TICK_PER_SECOND * 0.5);
        }
    }
}

void ppp_link_status_cb(void *ctx, int errCode, void *arg) {
    if (errCode == RT_EOK) {
        rt_kprintf("ppp link success.\n");

        rt_thread_t network_thread;

        network_thread = rt_thread_create(
                "network_thread",
                network_test_thread_entry, // void (*entry)(void *parameter),
                RT_NULL, // void       *parameter,
                512, // rt_uint32_t stack_size,
                20, // rt_uint8_t  priority,
                10 // rt_uint32_t tick);
        );
        if (network_thread != RT_NULL) {
            rt_thread_startup(network_thread);
        }
    }
}

void led_thread_entry(void *param) {
    rt_kprintf("led_thread_start.\r\n");
    while (1) {
        GPIO_setOutputHighOnPin(LED_PORT, LED_R);
        rt_thread_delay(RT_TICK_PER_SECOND * 0.2);
        GPIO_setOutputLowOnPin(LED_PORT, LED_R);
        rt_thread_delay(RT_TICK_PER_SECOND * 0.8);
    }
}

void mdm_thread_entry(void *param) {
    rt_device_t mdm_dev;
    rt_err_t res;
    char *dial_cmd = "ATDT*99***1#\r\n";
    char rx_buff[32];
    int rx_size = 0;

    rt_thread_delay(RT_TICK_PER_SECOND * 10);
    mdm_dev = rt_device_find("serial2");
    if (mdm_dev == RT_NULL) {
        rt_kprintf("mdm_thread start fail.\n");
        return;
    }
    res = rt_device_open(mdm_dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (res != RT_EOK) {
        rt_kprintf("mdm_thread open mdm_dev fail.\n");
        return;
    }
    rt_device_write(mdm_dev, 0, dial_cmd, rt_strlen(dial_cmd));
    WAIT_START:
    rt_thread_delay(RT_TICK_PER_SECOND * 5);
    rt_memset(rx_buff, 0x00, sizeof(rx_buff));
    rx_size = rt_device_read(mdm_dev, 0, rx_buff, sizeof(rx_buff));
    if (rx_size > 0) {
        rt_kprintf("mdm_thread rx_buff [%s].\n", rx_buff);
        if (rt_strstr(rx_buff, "CONN") == RT_NULL) {
            rt_kprintf("mdm_thread dial fail.\n");
            goto WAIT_START;
        }
        else {
            extern void lwip_system_init(void);
            lwip_system_init();

            pppInit();
            pppSetAuth(PPPAUTHTYPE_ANY, "", "");

            pppOverSerialOpen(mdm_dev, ppp_link_status_cb, RT_NULL);
        }
    }
    else {
        rt_kprintf("mdm_thread rx fail.");
        goto WAIT_START;
    }
}

extern void user_application_init(void);

void init_thread_entry(void *param) {

    user_application_init();

    // if (0) {
    //     rt_thread_t mdm_thread;

    //     mdm_thread = rt_thread_create(
    //             "mdm_thread",
    //             mdm_thread_entry, // void (*entry)(void *parameter),
    //             RT_NULL, // void       *parameter,
    //             1024, // rt_uint32_t stack_size,
    //             20, // rt_uint8_t  priority,
    //             10 // rt_uint32_t tick);
    //     );
    //     if (mdm_thread != RT_NULL) {
    //         rt_thread_startup(mdm_thread);
    //     }
    // }
}

void rt_application_init(void) {
    rt_thread_t init_thread;
    rt_thread_t led_thread;

    /*
    init_thread = rt_thread_create(
            "init_thread",
            init_thread_entry, // void (*entry)(void *parameter),
            RT_NULL, // void       *parameter,
            2048, // rt_uint32_t stack_size,
            20, // rt_uint8_t  priority,
            10 // rt_uint32_t tick);
    );
    if (init_thread != RT_NULL) {
        rt_thread_startup(init_thread);
    }
    */

    led_thread = rt_thread_create(
            "led_thread",
            led_thread_entry, // void (*entry)(void *parameter),
            RT_NULL, // void       *parameter,
            512, // rt_uint32_t stack_size,
            20, // rt_uint8_t  priority,
            10 // rt_uint32_t tick);
    );
    if (led_thread != RT_NULL) {
        rt_thread_startup(led_thread);
    }
}

void rtthread_startup(void) {
    rt_hw_board_init();

    rt_show_version();

    rt_system_tick_init();

    rt_system_object_init();

    rt_system_timer_init();

    rt_system_heap_init(&__heap_start__, (void *) (0x20000000 + 64 * 1024));
    // rt_system_heap_init(&Image$$RW_IRAM1$$ZI$$Limit, (void *) (0x20000000 + 64 * 1024));
    // rt_system_heap_init(__segment_end("HEAP"), (void *) (0x20000000 + 64 * 1024));


    rt_system_scheduler_init();

    rt_application_init();

#ifdef RT_USING_FINSH
    finsh_system_init();
    finsh_set_device("serial1");
#endif

    rt_system_timer_thread_init();

    rt_thread_idle_init();

    rt_system_scheduler_start();
}

int main(void) {
    rt_hw_interrupt_disable();

    rtthread_startup();

    return 0;
}
