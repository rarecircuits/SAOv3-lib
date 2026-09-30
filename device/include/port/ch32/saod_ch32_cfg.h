/**
 * @file port/ch32/saod_ch32_cfg.h
 *
 * @brief Holds port-specific configuration for the WCH CH32 port of the SAO Device Library (saod)
 *
 * Included by saod_port_cfg.h when this port is selected. Do not include directly.
 * Every value here can be overridden from your saod_user_cfg.h (which is included first) or the command line.
 */

#ifndef SAOD_CH32_CFG_H
#define SAOD_CH32_CFG_H

// ========================================
// Core Feature Support
// ========================================

// These are port defaults, so they are guarded. saod_user_cfg.h is included
// first, and an unguarded #define here would override it while warning about
// the redefinition - contradicting the note at the top of this file.

// The CH32 I2C peripheral detects STOP in slave mode (STAR1.STOPF), so PEC works
#ifndef SAOD_CFG_PEC_ENABLED
#define SAOD_CFG_PEC_ENABLED 1
#endif

// Dual addressing (OADDR2.ENDUAL) lets us listen on the ARP address and our assigned address at once.
// Set to 0 to answer only on the configured address, held in OADDR1 with the
// dual address slot unused - needed on parts where dual addressing misbehaves.
#ifndef SAOD_CFG_ENABLE_ARP
#define SAOD_CFG_ENABLE_ARP 1
#endif


// ========================================
// Chip Selection
// ========================================

// SPL header providing I2C_TypeDef and the I2C_* register bit definitions.
// PlatformIO's platform-ch32v already puts Peripheral/<series>/inc on the include path.
#ifndef SAOD_CH32_DEVICE_HEADER
#if defined(CH32X03x) || defined(CH32X035)
#define SAOD_CH32_DEVICE_HEADER "ch32x035.h"
#elif defined(CH32V00x) || defined(CH32V003)
#define SAOD_CH32_DEVICE_HEADER "ch32v00x.h"
#elif defined(CH32V20x)
#define SAOD_CH32_DEVICE_HEADER "ch32v20x.h"
#elif defined(CH32V30x)
#define SAOD_CH32_DEVICE_HEADER "ch32v30x.h"
#else
// Note: no apostrophes or quotes in the message below - the preprocessor lexes skipped branches too
#error Unknown CH32 family. Define SAOD_CH32_DEVICE_HEADER to the SPL header for your chip, e.g. ch32x035.h
#endif
#endif

// I2C peripheral instance the SAO listens on
#ifndef SAOD_CH32_I2C
#define SAOD_CH32_I2C I2C1
#endif

// SMBus bus speed. SMBus tops out at 100kHz, so there is rarely a reason to change this.
#ifndef SAOD_CH32_I2C_CLOCK_HZ
#define SAOD_CH32_I2C_CLOCK_HZ 100000
#endif

// Address of the factory-programmed 96-bit unique ID (ESIG_UNIID) used to derive the SAO serial number.
// This lives at the same address across the CH32 families.
#ifndef SAOD_CH32_UNIQUE_ID_ADDR
#define SAOD_CH32_UNIQUE_ID_ADDR 0x1FFFF7E8UL
#endif


// ========================================
// ARP Get UDID bit-bang workaround
// ========================================

// The CH32 I2C slave cannot report arbitration loss -- the registers ST's part has for it are simply absent, despite
// the product brief implying otherwise. That only matters for one exchange: during an SMBus ARP Get UDID every
// unassigned device answers at once and the wired-AND on SDA decides the winner. A device that cannot tell it lost
// keeps driving, talks over whoever actually won, and the enumeration comes back garbage.
//
// With this enabled the peripheral still does the address match, and then the port takes the bus over in GPIO
// open-drain mode and clocks the response out by hand, checking every bit it leaves high for another device pulling
// it low. On losing it goes silent for the rest of the transaction, which is what real arbitration would have done.
//
// Only the transmit side needs this. Prepare to ARP and Assign Address are writes, so the peripheral still handles
// them normally.
#ifndef SAOD_CH32_ARP_BITBANG_UDID
#define SAOD_CH32_ARP_BITBANG_UDID 0
#endif

// Which pins the I2C peripheral above is wired to. Only the bit-bang workaround needs these, since it has to flip
// them between alternate-function and GPIO. Defaults are the CH32V003's I2C1 mapping with no remap.
//
// Both pins must sit in the low half of the port: mode switching goes through CFGLR only, which is all the CH32V003
// has in any case.
#ifndef SAOD_CH32_I2C_PORT
#define SAOD_CH32_I2C_PORT GPIOC
#endif
#ifndef SAOD_CH32_I2C_SCL_PIN
#define SAOD_CH32_I2C_SCL_PIN 2
#endif
#ifndef SAOD_CH32_I2C_SDA_PIN
#define SAOD_CH32_I2C_SDA_PIN 1
#endif

// Upper bound on how long the bit-bang will wait for any ONE clock edge, counted in polling iterations rather than
// time. It exists only so a master that dies mid-transfer cannot wedge the device -- it is not a timing reference,
// and the master sets the actual bit rate.
//
// Per edge, not per transfer, and deliberately small. This runs with interrupts disabled, so the budget is also how
// long the device is deaf to everything else after a master gives up. A whole-transfer budget of 2000000 spins meant
// roughly 400ms of that, which is long enough that the host's next attempt finds nothing listening and fails too --
// one glitched transfer turned into a permanent failure. At ~0.23us per iteration this is about 4.6ms, against the
// 5us an edge actually takes at 100kHz.
#ifndef SAOD_CH32_BITBANG_TIMEOUT_SPINS
#define SAOD_CH32_BITBANG_TIMEOUT_SPINS 20000UL
#endif

// How long the tick will stay with a write phase waiting for the next byte, counted in polling iterations. Reset
// every time a byte arrives, so it bounds the gap between bytes rather than the length of the transfer.
//
// A byte left sitting in RXNE does not survive the repeated START that follows it, and the peripheral raises no
// error when it goes. The gap between a command byte's last clock and that repeated START is a couple of
// microseconds, so no polling rate makes catching it reliable -- the tick has to stay with the transfer instead.
// At ~0.23us per iteration this is about 460us, against the 90us a byte takes at 100kHz.
#ifndef SAOD_CH32_WRITE_PHASE_SPINS
#define SAOD_CH32_WRITE_PHASE_SPINS 2000UL
#endif

// How long the tick will keep waiting for the bus to go idle after this device NAKs a byte, in spins of the same
// loop. Separate from the byte-gap budget above because it bounds something different: not the gap between bytes,
// but the tail of a whole transfer this device has already dropped out of. Once ACK is clear the peripheral raises
// no further RXNE, so the byte-gap guard would expire mid-transfer and leave the device deaf for the remainder.
//
// It has to cover the longest transfer another device might be having: an ARP Assign Address is 20 bytes, ~1.8ms at
// 100kHz. At ~0.23us per iteration 20000 is about 4.6ms, comfortably clear of that and far short of the 35ms SMBus
// timeout. Only reached while ACK is clear, when there is no other bus work this device could be doing anyway.
#ifndef SAOD_CH32_NAK_IDLE_SPINS
#define SAOD_CH32_NAK_IDLE_SPINS 20000UL
#endif

// How many ticks ACK may stay cleared after this device NAKs a byte before it is put back regardless of the bus.
//
// The preferred restore point is bus idle, but a NAK raised by a tick that landed mid-transfer reaches the end of the
// tick with BUSY still set, and waiting only for idle leaves the device deaf for the rest of that transfer plus
// another poll interval -- long enough to miss the host's next transaction. Blocking until idle instead is worse: on
// a busy bus every tick burns its whole budget and the device stops answering entirely.
//
// A few ticks is a couple of hundred microseconds at any sane poll rate, and restoring early is harmless: SDA is
// wired-AND, so this device's NAK is invisible whenever another device acknowledges the same byte.
#ifndef SAOD_CH32_NAK_MAX_TICKS
#define SAOD_CH32_NAK_MAX_TICKS 4
#endif

// Log the peripheral status register on every tick into a ring that freezes on the first failure. Off by default:
// it costs a store on the tick, which is the hot path. Turn it on to diagnose a transfer that goes wrong in a way
// the counters cannot explain.
#ifndef SAOD_CH32_DEBUG_STAR_LOG
#define SAOD_CH32_DEBUG_STAR_LOG 0
#endif


// ========================================
// Resource Limits
// ========================================

// The library default (255) costs 256 bytes of RAM for the SMBus scratch buffer, which is far too much for the
// smaller parts. The library floor is 32 bytes; that still fits every common interface response (the largest is
// the ARP UDID + address at 17 bytes).
#ifndef SAOD_CFG_MAX_SMBUS_BLOCKLEN
#if defined(CH32V00x) || defined(CH32V003)
#define SAOD_CFG_MAX_SMBUS_BLOCKLEN 32 /* CH32V003: 2K RAM total, 256 byte stack */
#else
#define SAOD_CFG_MAX_SMBUS_BLOCKLEN 64
#endif
#endif

#endif
