/**
 * @file board.h
 * @brief Pin assignments for the example
 *
 * Defaults are for the CH32X035F8U6-EVT-R0. Check these against your own board before flashing - packages and
 * pinouts differ across the CH32 families.
 */

#ifndef BOARD_H
#define BOARD_H

#if defined(CH32V00x) || defined(CH32V003)

/* ---------- CH32V003 ---------- */

// I2C1, default pin mapping (no AFIO remap): SCL PC2, SDA PC1.
#define BOARD_I2C_PORT GPIOC
#define BOARD_I2C_SCL_PIN 2
#define BOARD_I2C_SDA_PIN 1

// This example drives a WS2812 string through ws2812.c, so there is no LED pin here. It uses PC6 (SPI1
// MOSI), the pin that can stream WS2812 data over SPI+DMA on the CH32V003, as ch32fun's driver does.
// An SAO with a plain LED instead would define BOARD_LED_PORT/PIN here like the CH32X035 section below.

// SAO GPIO1, used by the Port Identify interface.
// Note PD1 is also SWIO on this part: once it is configured as a GPIO the debug probe loses the target.
#define BOARD_GPIO1_PORT GPIOD
#define BOARD_GPIO1_PIN 1

#else

/* ---------- CH32X035F8U6-EVT-R0 (upstream default) ---------- */

// I2C1, default pin mapping (no AFIO remap): SCL on PA10, SDA on PA11.
// If your board routes I2C elsewhere, remap in board_init() and adjust these.
#define BOARD_I2C_PORT GPIOA
#define BOARD_I2C_SCL_PIN 10
#define BOARD_I2C_SDA_PIN 11

// The LED presented over the SAO LED interface
#define BOARD_LED_PORT GPIOA
#define BOARD_LED_PIN 0

// SAO GPIO1, used by the Port Identify interface
#define BOARD_GPIO1_PORT GPIOA
#define BOARD_GPIO1_PIN 2

#endif

// GPIO CFGLR/CFGHR nibble values
#define BOARD_CFG_INPUT_FLOATING 0x4
#define BOARD_CFG_OUTPUT_PP_10MHZ 0x1
#define BOARD_CFG_OUTPUT_AF_OD_50MHZ 0xF

// Sets one pin's 4-bit config field. Pins 0-7 live in CFGLR, 8-15 in CFGHR, 16-23 in CFGXR.
// CH32X035 ports run up to 24 pins, so CFGXR matters here in a way it does not on most CH32 parts.
//
// The CH32V003 has 8 pins per port and its GPIO_TypeDef declares CFGLR only, so
// referring to CFGHR/CFGXR there is a compile error rather than dead code.
static inline void board_pin_cfg(GPIO_TypeDef *port, uint8_t pin, uint32_t cfg)
{
    volatile uint32_t *reg;
    uint8_t shift = (pin & 7) * 4;

#if defined(CH32V00x) || defined(CH32V003)
    (void) pin;
    reg = &port->CFGLR;
#else
    if (pin < 8) {
        reg = &port->CFGLR;
    }
    else if (pin < 16) {
        reg = &port->CFGHR;
    }
    else {
        reg = &port->CFGXR;
    }
#endif

    *reg = (*reg & ~((uint32_t) 0xF << shift)) | (cfg << shift);
}

static inline void board_pin_write(GPIO_TypeDef *port, uint8_t pin, uint8_t level)
{
    port->BSHR = (uint32_t) 1 << (level ? pin : (pin + 16));
}

#endif
