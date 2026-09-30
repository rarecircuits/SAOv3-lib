#include "saod_attiny.h"
#include "usart.h"

#include <avr/io.h>

#define LED1_PORT PORTA
#define LED1_PIN PIN3_bm
#define LED2_PORT PORTA
#define LED2_PIN PIN4_bm
#define LED3_PORT PORTA
#define LED3_PIN PIN5_bm
#define LED4_PORT PORTC
#define LED4_PIN PIN0_bm
#define LED5_PORT PORTC
#define LED5_PIN PIN1_bm

void saod_itfled_update_led_cb(led_cmd_t *cmd_arr, uint16_t cmd_arr_cnt, uint16_t new_cmd_idx, uint16_t new_cmd_cnt)
{
    (void) new_cmd_idx;
    (void) new_cmd_cnt;

    if (cmd_arr_cnt != 5)
        return;

    if (cmd_arr[0])
        LED1_PORT.OUTSET = LED1_PIN;
    else
        LED1_PORT.OUTCLR = LED1_PIN;

    if (cmd_arr[1])
        LED2_PORT.OUTSET = LED2_PIN;
    else
        LED2_PORT.OUTCLR = LED2_PIN;

    if (cmd_arr[2])
        LED3_PORT.OUTSET = LED3_PIN;
    else
        LED3_PORT.OUTCLR = LED3_PIN;

    if (cmd_arr[3])
        LED4_PORT.OUTSET = LED4_PIN;
    else
        LED4_PORT.OUTCLR = LED4_PIN;

    if (cmd_arr[4])
        LED5_PORT.OUTSET = LED5_PIN;
    else
        LED5_PORT.OUTCLR = LED5_PIN;
}


int main(void)
{
    CCP = 0xD8;
    CLKCTRL.MCLKCTRLB = 0;  // no prescaler

    USART0_init();
    USART0_sendString("\r\nStartup (SAO test)\r\n");

    LED1_PORT.DIRSET = LED1_PIN;
    LED2_PORT.DIRSET = LED2_PIN;
    LED3_PORT.DIRSET = LED3_PIN;
    LED4_PORT.DIRSET = LED4_PIN;
    LED5_PORT.DIRSET = LED5_PIN;

    saod_usercode_set_led(0, 1);

    saod_attiny_init();

    USART0_sendString("Starting SMBus Device\r\n");

    while (1) {
        saod_attiny_tick();
    }
}
