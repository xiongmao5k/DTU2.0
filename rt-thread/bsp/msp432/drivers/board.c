#include "board.h"
#include "hw_serial.h"

#include "driverlib.h"

#include "rthw.h"
#include "rtthread.h"

void systick_interrupt_handler(void) {
    // static int cnt = 0;
    // switch (cnt++ % 3) {
    //     case 0:
    //         GPIO_setOutputLowOnPin(LED_PORT, LED_G);
    //         GPIO_setOutputHighOnPin(LED_PORT, LED_R);
    //         break;
    //     case 1:
    //         GPIO_setOutputLowOnPin(LED_PORT, LED_R);
    //         GPIO_setOutputHighOnPin(LED_PORT, LED_B);
    //         break;
    //     case 2:
    //         GPIO_setOutputLowOnPin(LED_PORT, LED_B);
    //         GPIO_setOutputHighOnPin(LED_PORT, LED_G);
    //         break;
    //
    // }
    rt_interrupt_enter();

    rt_tick_increase();

    rt_interrupt_leave();
}

void systick_init(void) {
    SysTick_enableModule();
    SysTick_setPeriod(CS_getMCLK() / RT_TICK_PER_SECOND);
    SysTick_registerInterrupt(systick_interrupt_handler);
    SysTick_enableInterrupt();
}

void gpio_init(void) {
    /* configure uart0 gpio */
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P1, GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);
    /* configure uart1 gpio */
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2, GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);
    /* configure cs gpio */
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_PJ, GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_PJ, GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);
    /* configure led gpio */
    // GPIO_setAsOutputPin(LED_PORT, LED_B | LED_G | LED_R);
    // GPIO_setOutputLowOnPin(LED_PORT, LED_R);
    // GPIO_setOutputLowOnPin(LED_PORT, LED_B);
    // GPIO_setOutputLowOnPin(LED_PORT, LED_G);
    GPIO_setAsOutputPin(LED_PORT, LED_G | LED_R);
    GPIO_setOutputLowOnPin(LED_PORT, LED_R);
    GPIO_setOutputLowOnPin(LED_PORT, LED_G);
}

void cs_init(void) {
    FlashCtl_setWaitState(FLASH_BANK0, 2);
    FlashCtl_setWaitState(FLASH_BANK1, 2);

    PCM_setCoreVoltageLevel(PCM_VCORE1);
    CS_setExternalClockSourceFrequency(32768, 48000000);
    if (CS_startHFXT(false) == true) {
        CS_initClockSignal(CS_HSMCLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);

        CS_initClockSignal(CS_SMCLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_2);
        CS_initClockSignal(CS_MCLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);
    } else {
        while (1);
    }
}


void rt_hw_board_init(void) {
    systick_init();
    gpio_init();
    // cs_init();

    //rt_hw_serial_init();
    rt_console_set_device("serial1");
}
