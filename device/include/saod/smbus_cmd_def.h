/**
 * @file saod/smbus_cmd_def.h
 * @brief Contains SMBus Command/Protocol Definitions for the SAO Device Library
 *
 * These definitions are structured in a way to make defining SMBus command processing logic simple, while keeping
 * the core SMBus handling logic relatively lightweight.
 *
 * At a high level, each SMBus command can be broken down into various protocol phases which can be optionally present
 * depending on the command type. This file defines each SMBus command type as a protocol, composed of these various
 * phase definitions. Then each of these command types has a corresponding function typedef that is called for that
 * command type.
 *
 * The command definition object is the interface that SMBus commands are defined for the core handling logic. A command
 * definition is just a function pointer, and a corresponding protocol enum that tells the saod core how to process the
 * bytes for that SMBus command and pass it to the command callback.
 *
 * Callbacks always fire at the end of I2C write phase of the SMBus command (and before the read phase if present).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_SMBUS_PROTO_H
#define SAOD_SMBUS_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========================================
// Internal Protocol Handling Flags
// ========================================

#define PROTO_FIXEDLEN_MASK 0xF
#define PROTO_HAS_FIXEDLEN(len) (len & PROTO_FIXEDLEN_MASK)
#define PROTO_HAS_WRITE_PHASE (1u << 4)
#define PROTO_HAS_READ_PHASE (1u << 5)

// Internal flag to handle assign address backoff. Do not use unless you're the ARP driver
#define PROTO_INTERNAL_CMD_ASSIGN_ADDR_FLAG (1 << 6)


// ========================================
// SMBus Protocol (Command Type) Definition
// ========================================

/**
 * @brief SMBus protocol types that an SMBus command can implement.
 * See the SMBus specification for a description on what each protocol type looks like on the wire
 *
 * @note When assigning to command structs, this MUST match the function type used
 * in the callback.
 */
enum smbus_xfer_protocol {
    // No arguments (except for the command)
    PROTO_SIMPLE_CMD = 0,  // Technically called Send Byte in SMBus spec but that's confusing

    // Host -> Target only
    PROTO_WRITE_BYTE = PROTO_HAS_WRITE_PHASE | PROTO_HAS_FIXEDLEN(1),
    PROTO_WRITE_WORD = PROTO_HAS_WRITE_PHASE | PROTO_HAS_FIXEDLEN(2),
    PROTO_WRITE_DATA32 = PROTO_HAS_WRITE_PHASE | PROTO_HAS_FIXEDLEN(4),
    PROTO_WRITE_BLOCK = PROTO_HAS_WRITE_PHASE,

    // Target -> Host only (except command)
    PROTO_READ_BYTE = PROTO_HAS_READ_PHASE | PROTO_HAS_FIXEDLEN(1),
    PROTO_READ_WORD = PROTO_HAS_READ_PHASE | PROTO_HAS_FIXEDLEN(2),
    PROTO_READ_DATA32 = PROTO_HAS_READ_PHASE | PROTO_HAS_FIXEDLEN(4),
    PROTO_READ_BLOCK = PROTO_HAS_READ_PHASE,

    // Bidrectional Traffic
    PROTO_PROCESS_CALL = PROTO_HAS_READ_PHASE | PROTO_HAS_WRITE_PHASE | PROTO_HAS_FIXEDLEN(2),
    PROTO_PROCESS_CALL_BLOCK = PROTO_HAS_READ_PHASE | PROTO_HAS_WRITE_PHASE,

    // Internal ARP Assign Address Proto
    PROTO_INTERNAL_CMD_ASSIGN_ADDR = PROTO_INTERNAL_CMD_ASSIGN_ADDR_FLAG | PROTO_HAS_WRITE_PHASE
};


// ========================================
// Command Callback Definitions
// ========================================

typedef void (*smbus_cb_simple_cmd)(void);
typedef void (*smbus_cb_write_byte)(uint8_t data);
typedef void (*smbus_cb_write_word)(uint16_t data);
typedef void (*smbus_cb_write_data32)(uint32_t data);
typedef void (*smbus_cb_write_block)(const uint8_t *data, uint8_t data_len);
typedef uint8_t (*smbus_cb_read_byte)(void);
typedef uint16_t (*smbus_cb_read_word)(void);
typedef uint32_t (*smbus_cb_read_data32)(void);
typedef uint8_t (*smbus_cb_read_block)(uint8_t *data_out);
typedef uint16_t (*smbus_cb_process_call)(uint16_t data_in);
typedef uint8_t (*smbus_cb_process_call_block)(uint8_t *data_in_out, uint8_t data_in_len);

union smbus_cmd_cb {
    smbus_cb_simple_cmd simple_cmd_cb;
    smbus_cb_write_byte write_byte_cb;
    smbus_cb_write_word write_word_cb;
    smbus_cb_write_data32 write_data32_cb;
    smbus_cb_write_block write_block_cb;
    smbus_cb_read_byte read_byte_cb;
    smbus_cb_read_word read_word_cb;
    smbus_cb_read_data32 read_data32_cb;
    smbus_cb_read_block read_block_cb;
    smbus_cb_process_call process_call_cb;
    smbus_cb_process_call_block process_call_block_cb;
    void *raw_ptr;
};


// ========================================
// SMBus Command Definition
// ========================================

typedef struct smbus_cmd_def {
    enum smbus_xfer_protocol proto;
    union smbus_cmd_cb cb;
} smbus_cmd_def_t;

// Each of these macros defines a smbus_cmd_def_t for the specified command type

// clang-format off
#define DEFINE_UNDEFINED_CMD { .proto = PROTO_SIMPLE_CMD, .cb = { .simple_cmd_cb = NULL }}
#define DEFINE_SIMPLE_CMD(cb_func) { .proto = PROTO_SIMPLE_CMD, .cb = { .simple_cmd_cb = (cb_func) }}
#define DEFINE_WRITE_BYTE_CMD(cb_func) { .proto = PROTO_WRITE_BYTE, .cb = { .write_byte_cb = (cb_func) }}
#define DEFINE_WRITE_WORD_CMD(cb_func) { .proto = PROTO_WRITE_WORD, .cb = { .write_word_cb = (cb_func) }}
#define DEFINE_WRITE_DATA32_CMD(cb_func) { .proto = PROTO_WRITE_DATA32, .cb = { .write_data32_cb = (cb_func) }}
#define DEFINE_WRITE_BLOCK_CMD(cb_func) { .proto = PROTO_WRITE_BLOCK, .cb = { .write_block_cb = (cb_func) }}
#define DEFINE_READ_BYTE_CMD(cb_func) { .proto = PROTO_READ_BYTE, .cb = { .read_byte_cb = (cb_func) }}
#define DEFINE_READ_WORD_CMD(cb_func) { .proto = PROTO_READ_WORD, .cb = { .read_word_cb = (cb_func) }}
#define DEFINE_READ_DATA32_CMD(cb_func) { .proto = PROTO_READ_DATA32, .cb = { .read_data32_cb = (cb_func) }}
#define DEFINE_READ_BLOCK_CMD(cb_func) { .proto = PROTO_READ_BLOCK, .cb = { .read_block_cb = (cb_func) }}
#define DEFINE_PROCESS_CALL_CMD(cb_func) { .proto = PROTO_PROCESS_CALL, .cb = { .process_call_cb = (cb_func) }}
#define DEFINE_PROCESS_CALL_BLOCK_CMD(cb_func) { .proto = PROTO_PROCESS_CALL_BLOCK, .cb = { .process_call_block_cb = (cb_func) }}
// clang-format on

#ifdef __cplusplus
}
#endif

#endif /* WIDGET_H */
