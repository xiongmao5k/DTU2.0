//
// Created by zhang on 2017/7/19.
//
#include <msp432_driverlib_3_21_00_05/inc/msp.h>
#include "driverlib.h"

#include "hw_serial.h"

#include "rtdevice.h"
#include "board.h"

#define serial_get_flag(flg) (EUSCI_A_CMSIS(uart->id)->IFG & (flg))
#define serial_clean_flg(flg) (EUSCI_A_CMSIS(uart->id)->IFG &= (~(flg)))

struct msp432_uart {
    unsigned id;
    unsigned irq;
    eUSCI_UART_Config conf;
};

static rt_err_t msp432_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct msp432_uart *uart;
    eUSCI_UART_Config *conf;

    uart = (struct msp432_uart *)serial->parent.user_data;
    conf = &uart->conf;

    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);


    switch (cfg->baud_rate) {
        case BAUD_RATE_115200:
            conf->clockPrescalar    = 13;
            conf->firstModReg       = 0;
            conf->secondModReg      = 37;
            conf->overSampling      = 1;
            break;
        case BAUD_RATE_9600:
        default:
            conf->clockPrescalar    = 156;
            conf->firstModReg       = 4;
            conf->secondModReg      = 0;
            conf->overSampling      = 1;
            break;
    }

    switch (cfg->parity) {
        case PARITY_EVEN:
            conf->parity = EUSCI_A_UART_EVEN_PARITY;
            break;
        case PARITY_ODD:
            conf->parity = EUSCI_A_UART_ODD_PARITY;
            break;
        case PARITY_NONE:
        default:
            conf->parity = EUSCI_A_UART_NO_PARITY;
            break;
    }

    switch (cfg->stop_bits) {
        case STOP_BITS_2:
            conf->numberofStopBits = EUSCI_A_UART_TWO_STOP_BITS;
            break;
        case STOP_BITS_3:
        case STOP_BITS_4:
        case STOP_BITS_1:
        default:
            conf->numberofStopBits = EUSCI_A_UART_ONE_STOP_BIT;
            break;
    }

    switch (cfg->data_bits) {
        case DATA_BITS_5:
        case DATA_BITS_6:
        case DATA_BITS_7:
        case DATA_BITS_8:
        case DATA_BITS_9:
        default:
            break;
    }

    // conf->msborLsbFirst = EUSCI_A_UART_MSB_FIRST;
    // conf->uartMode = EUSCI_A_UART_MODE;

    UART_disableModule(uart->id);

    UART_initModule(uart->id, conf);

    UART_enableModule(uart->id);

    // UART_enableInterrupt(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT);

    return RT_EOK;
}

static rt_err_t msp432_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct msp432_uart *uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct msp432_uart *)serial->parent.user_data;

    switch (cmd)
    {
        /* disable interrupt */
        case RT_DEVICE_CTRL_CLR_INT:
            rt_kprintf("%s disable int.", serial->parent.parent.name);
            /* disable rx irq */
            UART_disableInterrupt(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT);
            /* disable interrupt */
            Interrupt_disableInterrupt(uart->irq);
            break;
            /* enable interrupt */
        case RT_DEVICE_CTRL_SET_INT:
            rt_kprintf("%s enable int.", serial->parent.parent.name);
            /* enable rx irq */
            UART_enableInterrupt(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT);
            /* enable interrupt */
            Interrupt_enableInterrupt(uart->irq);
            break;
        default:
            return RT_ERROR;
    }

    return RT_EOK;
}

static int msp432_putc(struct rt_serial_device *serial, char c)
{
    struct msp432_uart* uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct msp432_uart *)serial->parent.user_data;

    // UART_transmitData(uart->id, c);
    // while (!UART_getInterruptStatus(uart->id, EUSCI_A_UART_TRANSMIT_INTERRUPT));
    UART_clearInterruptFlag(uart->id, EUSCI_A_UART_TRANSMIT_COMPLETE_INTERRUPT_FLAG);
    EUSCI_A_CMSIS(uart->id)->TXBUF = c;
    while (!UART_getInterruptStatus(uart->id, EUSCI_A_UART_TRANSMIT_COMPLETE_INTERRUPT));

    return 1;
}

static int msp432_getc(struct rt_serial_device *serial)
{
    int ch = -1;
    struct msp432_uart* uart;

    RT_ASSERT(serial != RT_NULL);

    uart = (struct msp432_uart *)serial->parent.user_data;

    if (UART_getInterruptStatus(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT)) {
        ch = EUSCI_A_CMSIS(uart->id)->RXBUF;
        UART_clearInterruptFlag(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT);
    }

    return ch;
}

static const struct rt_uart_ops msp432_uart_ops = {
        msp432_configure,
        msp432_control,
        msp432_putc,
        msp432_getc,
};

struct msp432_uart msp432_uart1 = {
    EUSCI_A0_BASE,
    INT_EUSCIA0,
    {
            EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
            13,                                      // BRDIV = 78
            0,                                       // UCxBRF = 2
            37,                                       // UCxBRS = 0
            EUSCI_A_UART_NO_PARITY,                  // No Parity
            EUSCI_A_UART_LSB_FIRST,                  // LSB First
            EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
            EUSCI_A_UART_MODE,                       // UART mode
            EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION  // Oversampling
    }
};
struct rt_serial_device serial1;

void uart1_interrupt_handler(void)
{
    struct msp432_uart *uart;

    rt_interrupt_enter();

    uart = &msp432_uart1;

    if (UART_getInterruptStatus(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT))
    {
        rt_hw_serial_isr(&serial1, RT_SERIAL_EVENT_RX_IND);
    }

    rt_interrupt_leave();
}

struct msp432_uart msp432_uart2 = {
        EUSCI_A1_BASE,
        INT_EUSCIA1,
        {
                EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
                13,                                      // BRDIV = 78
                0,                                       // UCxBRF = 2
                37,                                       // UCxBRS = 0
                EUSCI_A_UART_NO_PARITY,                  // No Parity
                EUSCI_A_UART_LSB_FIRST,                  // LSB First
                EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
                EUSCI_A_UART_MODE,                       // UART mode
                EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION  // Oversampling
        }
};
struct rt_serial_device serial2;

void uart2_interrupt_handler(void)
{
    struct msp432_uart *uart;

    rt_interrupt_enter();

    uart = &msp432_uart2;

    if (UART_getInterruptStatus(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT))
    {
        rt_hw_serial_isr(&serial2, RT_SERIAL_EVENT_RX_IND);
    }

    rt_interrupt_leave();
}

void rt_hw_serial_init(void)
{
    struct msp432_uart *uart;
    eUSCI_UART_Config *conf;
    struct serial_configure cfg = RT_SERIAL_CONFIG_DEFAULT;

    uart = &msp432_uart1;
    conf = &uart->conf;

    cfg.baud_rate = BAUD_RATE_115200;
    serial1.ops = &msp432_uart_ops;
    serial1.config = cfg;

    // UART_initModule(uart->id, conf);

    // UART_enableModule(uart->id);

    UART_registerInterrupt(uart->id, uart1_interrupt_handler);
    // UART_enableInterrupt(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT);
    // Interrupt_enableInterrupt(uart->irq);

    rt_hw_serial_register(&serial1, "serial1", RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX, uart);


    uart = &msp432_uart2;
    conf = &uart->conf;

    cfg.baud_rate = BAUD_RATE_115200;
    cfg.bufsz = 1024;
    serial2.ops = &msp432_uart_ops;
    serial2.config = cfg;

    // UART_initModule(uart->id, conf);

    // UART_enableModule(uart->id);

    UART_registerInterrupt(uart->id, uart2_interrupt_handler);
    // UART_enableInterrupt(uart->id, EUSCI_A_UART_RECEIVE_INTERRUPT);
    // Interrupt_enableInterrupt(uart->irq);

    rt_hw_serial_register(&serial2, "serial2", RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX, uart);
}

