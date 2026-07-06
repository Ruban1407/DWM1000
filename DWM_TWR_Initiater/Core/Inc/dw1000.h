/******************************************************************************
 * dw1000.h
 *
 * DW1000 driver built on top of your existing spi_txrx() / DW1000_CS_LOW() /
 * DW1000_CS_HIGH() functions from main.c.
 ******************************************************************************/
#ifndef DW1000_H
#define DW1000_H

#include <stdint.h>
#include <stdbool.h>

/* ---- 40-bit timestamp type (we use 64-bit storage, top 24 bits unused) --- */
typedef uint64_t dw_timestamp_t;

/* ---- Basic SPI-level register access -------------------------------------*/
void     dw_write_reg(uint8_t reg_id, const uint8_t *data, uint16_t len);
void     dw_read_reg(uint8_t reg_id, uint8_t *data, uint16_t len);

void     dw_write_subreg(uint8_t reg_id, uint16_t sub_addr,
                          const uint8_t *data, uint16_t len);
void     dw_read_subreg(uint8_t reg_id, uint16_t sub_addr,
                         uint8_t *data, uint16_t len);

/* Convenience 32-bit / 16-bit helpers (little-endian on the wire) ----------*/
uint32_t dw_read_reg32(uint8_t reg_id);
void     dw_write_reg32(uint8_t reg_id, uint32_t value);
uint16_t dw_read_subreg16(uint8_t reg_id, uint16_t sub_addr);
void     dw_write_subreg16(uint8_t reg_id, uint16_t sub_addr, uint16_t value);
void     dw_write_subreg32(uint8_t reg_id, uint16_t sub_addr, uint32_t value);

/* ---- Init / reset ----------------------------------------------------------
 * dw_init() performs:
 *   1. Hardware reset (PB1)
 *   2. Device ID check
 *   3. The "default configurations that should be modified" fixups
 *      (section 2.5.5 of the user manual)
 *   4. LDE microcode load (Table 4, section 2.5.5.10)
 *   5. Antenna delay programming
 * Returns true if DEV_ID matched 0xDECA0130, false otherwise.
 *--------------------------------------------------------------------------*/
bool dw_init(void);
void dw_reset(void);

/* ---- Antenna delay (must be calibrated for your hardware, see notes) ----*/
void dw_set_tx_antenna_delay(uint16_t delay_ticks);
void dw_set_rx_antenna_delay(uint16_t delay_ticks);

/* ---- TX / RX basic frame operations ---------------------------------------
 * dw_send_frame(): blocking. Loads data into TX_BUFFER, sets TFLEN, fires
 *                  TXSTRT, waits for TXFRS.
 * dw_receive_frame(): blocking with timeout (in approx. microseconds, see
 *                  RX_FWTO units note in dw1000.c). Returns number of bytes
 *                  received (excluding 2-byte FCS), or -1 on error/timeout.
 *--------------------------------------------------------------------------*/
void  dw_send_frame(const uint8_t *data, uint16_t len);
int32_t dw_receive_frame(uint8_t *buf, uint16_t buf_len, uint32_t timeout_us);

/* ---- Timestamps ------------------------------------------------------------
 * Units: 1 tick = 1 / (128 * 499.2 MHz) seconds ~= 15.65 picoseconds
 * (Section 7.2.23 / 7.2.25 of the user manual)
 *--------------------------------------------------------------------------*/
dw_timestamp_t dw_read_tx_timestamp(void);
dw_timestamp_t dw_read_rx_timestamp(void);

/* ---- SS-TWR high level API -------------------------------------------------
 * Tick to metres conversion constant, derived below in dw1000.c
 *--------------------------------------------------------------------------*/

#define DW_TWR_FUNC_CODE_POLL   0x61U
#define DW_TWR_FUNC_CODE_RESP   0x62U

/* Initiator: sends Poll, waits for Response, computes distance in metres. */
bool dw_twr_initiator_range(float *distance_m);

/* Responder: waits for Poll, sends Response with embedded timestamps. Call
 * this in a loop on the anchor board. */
void dw_twr_responder_listen(void);

#endif /* DW1000_H */
