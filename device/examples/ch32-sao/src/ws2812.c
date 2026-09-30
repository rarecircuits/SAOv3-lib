/**
 * @file ws2812.c
 * @brief WS2812/SK6812 output via SPI1 MOSI + DMA on the CH32V003
 *
 * Register fields are written as explicit bit values rather than SPL macros:
 * the names for these vary between vendor header revisions, and the values do
 * not. Each one is spelled out in a comment.
 */

#include "saod.h"

#include SAOD_CH32_DEVICE_HEADER

#include "ws2812.h"

// Four SPI bits per LED bit, so one nibble of LED data becomes one 16-bit SPI
// word. '0' -> 1000, '1' -> 1110, MSB first.
static const uint16_t bit_quartets[16] = {
    0x8888, 0x888E, 0x88E8, 0x88EE, 0x8E88, 0x8E8E, 0x8EE8, 0x8EEE,
    0xE888, 0xE88E, 0xE8E8, 0xE8EE, 0xEE88, 0xEE8E, 0xEEE8, 0xEEEE,
};

// Trailing zero words hold the line low so the string latches. At 3 MHz a word
// is 5.33us, so 16 words is ~85us -- comfortably past the ~50us reset.
#define WS2812_RESET_WORDS 16

// 6 words per LED (3 bytes, 2 words each) plus the reset tail.
#define WS2812_BUF_WORDS ((WS2812_LED_COUNT * 6) + WS2812_RESET_WORDS)

static uint16_t ws2812_buf[WS2812_BUF_WORDS];

void ws2812_init(void)
{
    // Clocks: DMA1 on AHB (bit 0), GPIOC (bit 4) and SPI1 (bit 12) on APB2.
    RCC->AHBPCENR |= (1 << 0);
    RCC->APB2PCENR |= (1 << 4) | (1 << 12);

    // PC6 = alternate function push-pull, 10 MHz (nibble 0b1001 = 0x9).
    // Mask only PC6's nibble; the other pins on this port belong to other
    // peripherals.
    GPIOC->CFGLR = (GPIOC->CFGLR & ~((uint32_t) 0xF << (4 * 6))) | ((uint32_t) 0x9 << (4 * 6));

    // SPI1: master, 1-line transmit-only, 16-bit frames, software NSS,
    // fPCLK/16 = 3 MHz with a 48 MHz core.
    //   bit2 MSTR, bits5:3 BR=011 (/16), bit6 SPE, bit8 SSI, bit9 SSM,
    //   bit11 DFF (16-bit), bit14 BIDIOE, bit15 BIDIMODE
    SPI1->CTLR1 = 0xCB5C;

    // bit1 TXDMAEN: let DMA feed the transmit register.
    SPI1->CTLR2 = (1 << 1);

    // DMA1 channel 3 is SPI1_TX.
    DMA1_Channel3->PADDR = (uint32_t) &SPI1->DATAR;
    DMA1_Channel3->MADDR = (uint32_t) ws2812_buf;
    DMA1_Channel3->CNTR = 0;

    //   bit4 DIR=1 (memory -> peripheral), bit7 MINC, bits9:8 PSIZE=01 (16-bit),
    //   bits11:10 MSIZE=01 (16-bit), bits13:12 PL=11 (very high).
    // Circular mode is deliberately left off: one frame, one transfer.
    DMA1_Channel3->CFGR = (1 << 4) | (1 << 7) | (1 << 8) | (1 << 10) | (3 << 12);
}

int ws2812_busy(void)
{
    // Still counting down, or the last word has not clocked out of SPI yet
    // (STATR bit 7 = BSY).
    return (DMA1_Channel3->CNTR != 0) || (SPI1->STATR & (1 << 7));
}

void ws2812_show(const uint8_t *grb, unsigned n_leds)
{
    if (n_leds > WS2812_LED_COUNT) {
        n_leds = WS2812_LED_COUNT;
    }

    uint16_t *out = ws2812_buf;

    for (unsigned i = 0; i < n_leds * 3u; i++) {
        uint8_t byte = grb[i];
        *out++ = bit_quartets[byte >> 4];
        *out++ = bit_quartets[byte & 0x0F];
    }

    for (unsigned i = 0; i < WS2812_RESET_WORDS; i++) {
        *out++ = 0;
    }

    unsigned words = (unsigned) (out - ws2812_buf);

    // The channel must be disabled before CNTR can be written.
    DMA1_Channel3->CFGR &= ~(uint32_t) (1 << 0);
    DMA1_Channel3->MADDR = (uint32_t) ws2812_buf;
    DMA1_Channel3->CNTR = words;
    DMA1_Channel3->CFGR |= (1 << 0);
}
