/**
 * @file ws2812.h
 * @brief WS2812/SK6812 output on the CH32V003, via SPI1 MOSI and DMA
 *
 * Written against the vendor SPL headers rather than borrowed from ch32fun,
 * since this example builds under the noneos-sdk framework.
 *
 * Each LED bit becomes four SPI bits at 3 MHz (333ns each): 1000 for a zero,
 * 1110 for a one. That puts a bit period at 1.33us, inside the SK6812 window.
 *
 * The whole frame is encoded up front and pushed by a single one-shot DMA
 * transfer, so there is no refill interrupt and nothing to service mid-frame.
 * That matters here: the SAO library is polled from the main loop and must not
 * be starved, and a half-transfer ISR arriving late would corrupt pixel data.
 */

#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>

// Data out is PC6 (SPI1 MOSI), fixed by the peripheral.
#define WS2812_LED_COUNT 8

/**
 * @brief Configure SPI1, DMA1 channel 3, and PC6. Call once at startup.
 */
void ws2812_init(void);

/**
 * @brief Push a frame. Non-blocking: DMA carries it out in the background.
 *
 * @param grb  n_leds * 3 bytes, green/red/blue per LED. This matches the library's 8B_GRB led_cmd_t
 *             layout, so its array can be passed straight in.
 * @param n_leds Number of LEDs, up to WS2812_LED_COUNT.
 */
void ws2812_show(const uint8_t *grb, unsigned n_leds);

/**
 * @brief True while a frame is still going out. Do not call ws2812_show() then.
 */
int ws2812_busy(void);

#endif
