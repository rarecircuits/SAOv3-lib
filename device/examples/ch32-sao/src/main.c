/**
 * @file main.c
 * @brief Minimal SAOv3 device on a WCH CH32 part
 *
 * Blinks an LED on its own, and presents that LED to a host badge over the SAO LED interface so the badge can take
 * it over. Also implements Port Identify on GPIO1.
 *
 * The library is polled from the main loop, so the loop must never block for long - see saod_ch32_tick().
 */

#include "saod.h"

#include SAOD_CH32_DEVICE_HEADER

#include "board.h"
#include "debug.h"
#include "ws2812.h"

// Blink period while the SAO is driving its own LED
#define BLINK_INTERVAL_MS 500

// SysTick free-runs off HCLK/8
#define SYSTICK_HZ (SystemCoreClock / 8)


/* ============================================================
   Free running timebase

   Delay_Ms/Delay_Us busy-wait on SysTick in one-shot mode, which would starve the SAO poll loop, so they are only
   used during startup. From then on SysTick free-runs and the blink is paced off its counter.
   ============================================================ */
static void timebase_init(void)
{
    SysTick->CTLR = 0;
    SysTick->CNT = 0;
    SysTick->CMP = UINT64_MAX;  // Never reached, so no compare event fires
    SysTick->CTLR = 1;          // STE only: count up, HCLK/8, no reload, no interrupt
}

static inline uint32_t timebase_now(void)
{
    // Only the low word is needed; deltas below are computed with 32-bit wraparound arithmetic
    return *(volatile uint32_t *) &SysTick->CNT;
}


/* ============================================================
   SAO LED interface callback

   Called by the library whenever the LED needs refreshing, whether the value came from the blink below or from the
   host badge.
   ============================================================ */
void saod_itfled_update_led_cb(led_cmd_t *cmd_arr, uint16_t cmd_arr_cnt, uint16_t new_cmd_idx, uint16_t new_cmd_cnt)
{
    (void) new_cmd_idx;
    (void) new_cmd_cnt;

    // Always redraw the whole string. led_cmd_t in 8B_GRB mode is already the WS2812 wire order, so the
    // array is passed through as the frame.
    if (ws2812_busy()) {
        // Dropping this update is fine: the library keeps cmd_arr, and the next update redraws from it.
        return;
    }

    ws2812_show((const uint8_t *) cmd_arr, cmd_arr_cnt);
}


static void board_init(void)
{
    // Clocks for the GPIO ports in use, AFIO, and I2C1.
#if defined(CH32V00x) || defined(CH32V003)
    // I2C on GPIOC, SAO GPIO1 on GPIOD.
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPCEN | RCC_IOPDEN;
#else
    // Adjust if your board uses a port other than GPIOA.
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPAEN;
#endif
    RCC->APB1PCENR |= RCC_I2C1EN;

#ifndef BOARD_LED_PORT
    // Addressable LED string; ws2812_init() owns its pin.
    ws2812_init();
#else
    board_pin_cfg(BOARD_LED_PORT, BOARD_LED_PIN, BOARD_CFG_OUTPUT_PP_10MHZ);
#endif

    // SAO GPIO1 stays tristated until the badge asks for an output mode.
    board_pin_cfg(BOARD_GPIO1_PORT, BOARD_GPIO1_PIN, BOARD_CFG_INPUT_FLOATING);
}

// Must be called *after* saod_ch32_init has enabled the I2C peripheral - see the note in saod_ch32.h. An alternate
// function pin with its peripheral disabled outputs 0, which for open drain pulls the bus line to ground.
static void board_i2c_pins_init(void)
{
    // I2C pins must be alternate function open drain. Bus pull-ups come from the badge.
    board_pin_cfg(BOARD_I2C_PORT, BOARD_I2C_SCL_PIN, BOARD_CFG_OUTPUT_AF_OD_50MHZ);
    board_pin_cfg(BOARD_I2C_PORT, BOARD_I2C_SDA_PIN, BOARD_CFG_OUTPUT_AF_OD_50MHZ);
}


int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();

    board_init();

    // I2C1 is on APB1. On parts where APB1 is prescaled below HCLK, pass the real PCLK1 instead.
    saod_ch32_init(SystemCoreClock);

    // Only safe once the peripheral above is enabled
    board_i2c_pins_init();

    timebase_init();

    uint32_t blink_ticks = (SYSTICK_HZ / 1000) * BLINK_INTERVAL_MS;
    uint32_t next_blink = timebase_now();
    uint8_t led_state = 0;

    while (1) {
        // Services the I2C peripheral. Must run often: the peripheral stretches SCL until this catches up, so a
        // slow loop drags the bus down rather than losing data, but it must never stall long enough to time the
        // host out.
        saod_ch32_tick();

        if ((int32_t) (timebase_now() - next_blink) >= 0) {
            next_blink += blink_ticks;
            led_state = !led_state;

            // Dim warm white, to keep current draw modest.
            led_cmd_t frame[SAOD_CFG_ITFLED_COUNT];
            for (unsigned i = 0; i < SAOD_CFG_ITFLED_COUNT; i++) {
                frame[i].red = led_state ? 0x30 : 0x00;
                frame[i].green = led_state ? 0x18 : 0x00;
                frame[i].blue = led_state ? 0x08 : 0x00;
            }

            // Not a direct pin write: routing through the library lets the badge take the LEDs over via the LED
            // interface. While it holds control these updates are staged and only shown once it hands control back.
            saod_usercode_set_leds(frame, 0, SAOD_CFG_ITFLED_COUNT);
        }
    }
}
