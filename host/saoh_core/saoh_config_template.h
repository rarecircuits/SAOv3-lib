#ifndef SAOH_CONFIG_H
#define SAOH_CONFIG_H

/**
 * @brief Set to true if your platform supports SMBus block reads
 *
 * This is more efficient for performing block reads (does not require double reads/reading extra data).
 * However, most platforms do not support this
 */
#ifndef SAOH_CFG_SUPPORT_SMBUS_BLKREAD
#define SAOH_CFG_SUPPORT_SMBUS_BLKREAD 0
#endif


/**
 * @brief Configures the minimum system logging level
 */
#ifndef SAOH_CFG_LOG_MIN_LEVEL
#define SAOH_CFG_LOG_MIN_LEVEL LOGL_INFO
#endif

/**
 * @brief Number of times to retry critical commands (where failure can result in un-recoverable loss of functionality)
 */
#ifndef SAOH_CFG_CMD_RETRY_CNT
#define SAOH_CFG_CMD_RETRY_CNT 3
#endif

#endif
