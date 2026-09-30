#include <avr/io.h>

#define USART0_BAUD_RATE(baud) ((uint16_t) ((4UL * F_CPU) / (baud)))

void USART0_init(void)
{
    // Route USART0 to PORTB (PB2=TX, PB3=RX)
    // PORTMUX.USARTROUTEA = PORTMUX_USART0_DEFAULT_gc;

    // Set TX (PB2) as output
    PORTB.DIRSET = PIN2_bm;

    // 115200 baud @ 20 MHz
    // USART0.BAUD = 116;
    USART0.BAUD = USART0_BAUD_RATE(115200);

    // Enable transmitter
    USART0.CTRLB = USART_TXEN_bm;

    // 8 data bits, no parity, 1 stop bit (default)
}

void USART0_sendChar(char c)
{
    while (!(USART0.STATUS & USART_DREIF_bm))
        ;
    USART0.TXDATAL = c;
}

void USART0_sendString(const char *s)
{
    while (*s) {
        USART0_sendChar(*s++);
    }
}
