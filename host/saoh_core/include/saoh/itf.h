#ifndef SAOH_ITF_H
#define SAOH_ITF_H

#include "saoh/consts/err.h"
#include "saoh/smbus.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Verifies the requested SAO is a valid and runs a supported version
 *
 * @param bus Bus with SAO
 * @param pec_addr SAO address (and PEC mode)
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_itf_check_valid(saoh_bus_t *bus, uint8_t pec_addr);

/**
 * @brief Reads the requested SAO's VID, PID, and Firmware Version
 *
 * @param bus Bus with SAO
 * @param pec_addr SAO address (and PEC mode)
 * @param vid_out If non-null, the SAO VID is written to this pointer
 * @param pid_out If non-null, the SAO PID is written to this pointer
 * @param fw_version_out If non-null, the SAO Fiwmare Version is written to this pointer
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_itf_query_vidpid(saoh_bus_t *bus, uint8_t pec_addr, uint16_t *vid_out, uint16_t *pid_out,
                                 uint16_t *fw_version_out);

/**
 * @brief Read's the requested SAO's manufacturer string
 *
 * @param bus Bus with SAO
 * @param pec_addr SAO address (and PEC mode)
 * @param manufacturer_out Must be non-NULL char array of at least size SAO_CMNITF_STR_MAXLEN + 1
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_itf_query_manufacturer(saoh_bus_t *bus, uint8_t pec_addr, char *manufacturer_out);

/**
 * @brief Read's the requested SAO's name string
 *
 * @param bus Bus with SAO
 * @param pec_addr SAO address (and PEC mode)
 * @param name_out Must be non-NULL char array of at least size SAO_CMNITF_STR_MAXLEN + 1
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_itf_query_name(saoh_bus_t *bus, uint8_t pec_addr, char *name_out);

/**
 * @brief Read's the requested SAO's serial number
 *
 * @param bus Bus with SAO
 * @param pec_addr SAO address (and PEC mode)
 * @param serial_out Must be non-NULL pointer to uint32_t to write serial number
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_itf_query_serial(saoh_bus_t *bus, uint8_t pec_addr, uint32_t *serial_out);

#ifdef __cplusplus
}
#endif

#endif
