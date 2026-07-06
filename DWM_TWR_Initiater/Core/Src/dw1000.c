/*
 * dw1000.c
 *
 *  Created on: Jun 30, 2026
 *      Author: Ruban
 */


/******************************************************************************
 * dw1000.c
 *
 * Built directly on top of the spi_txrx()/CS functions you already have
 * working in main.c (the ones that successfully read DEV_ID = 0xDECA0130).
 *
 * All register addresses, bit positions, default-fixup values and the LDE
 * load sequence are taken verbatim from the DW1000 User Manual v2.18:
 *   - SPI transaction format:        Section 2.2.1.2, Figures 1-7
 *   - Default config fixups:         Section 2.5.5, pages 17-18
 *   - LDE microcode load sequence:   Table 4, page 18
 *   - TX_FCTRL / TX_BUFFER:          Section 7.2.10 / 7.2.11
 *   - SYS_CTRL bit positions:        Section 7.2.15
 *   - SYS_STATUS bit positions:      Section 7.2.17
 *   - RX_FINFO / RX_BUFFER:          Section 7.2.18 / 7.2.19
 *   - TX_TIME / RX_TIME:             Section 7.2.23 / 7.2.25
 *   - TX_ANTD / RX antenna delay:    Section 7.2.26, Section 8.3
 *   - SS-TWR formula:                Appendix 3, Section 12.2, Figure 36
 ******************************************************************************/
#include "dw1000.h"
#include "dw1000_regs.h"
#include "main.h"     /* for spi_txrx(), DW1000_CS_LOW/HIGH(), LL_mDelay */
#include <string.h>

/* These are defined in main.c — re-declared here so this file can call them */
extern uint8_t spi_txrx(uint8_t data);
extern void    DW1000_CS_LOW(void);
extern void    DW1000_CS_HIGH(void);

void UART_SendChar(uint8_t ch);
void UART_SendString(const char *str);
/* ===========================================================================
 *  Low level SPI transaction helpers
 *  (Section 2.2.1.2 of the user manual: 1, 2 or 3-octet header formats)
 * ===========================================================================
 */
volatile int32_t  rx_len_debug       = 0;
 volatile uint32_t sys_status_after_rx = 0;
 volatile uint32_t rx_finfo_debug      = 0;
 volatile uint64_t ticks_elapsed       = 0;
volatile uint32_t g_debug_status = 0;
/* Non-indexed (1-octet header) write/read -- Figure 2 / Figure 3 */
void dw_write_reg(uint8_t reg_id, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    DW1000_CS_LOW();
    spi_txrx(0x80U | (reg_id & 0x3FU));   /* bit7=1 (write), bit6=0 (no sub-idx) */
    for (i = 0; i < len; i++)
    {
        spi_txrx(data[i]);
    }
    DW1000_CS_HIGH();
}

void dw_read_reg(uint8_t reg_id, uint8_t *data, uint16_t len)
{
    uint16_t i;
    DW1000_CS_LOW();
    spi_txrx(0x00U | (reg_id & 0x3FU));   /* bit7=0 (read), bit6=0 (no sub-idx) */
    for (i = 0; i < len; i++)
    {
        data[i] = spi_txrx(0x00U);
    }
    DW1000_CS_HIGH();
}

/* Indexed write/read. Uses short (2-octet) header if sub_addr <= 0x7F,
 * otherwise the long (3-octet) header. (Figure 4-7) */
void dw_write_subreg(uint8_t reg_id, uint16_t sub_addr,
                      const uint8_t *data, uint16_t len)
{
    uint16_t i;
    DW1000_CS_LOW();

    if (sub_addr <= 0x7FU)
    {
        /* Short indexed header: bit7=1(write) bit6=1(sub-idx present) */
        spi_txrx(0xC0U | (reg_id & 0x3FU));
        spi_txrx((uint8_t)(sub_addr & 0x7FU));   /* bit7=0 -> short form */
    }
    else
    {
        /* Long indexed header: octet2 bit7=1 (extended), low7 bits = addr[6:0]
         * octet3 = addr[14:7] */
        spi_txrx(0xC0U | (reg_id & 0x3FU));
        spi_txrx((uint8_t)(0x80U | (sub_addr & 0x7FU)));
        spi_txrx((uint8_t)((sub_addr >> 7) & 0xFFU));
    }

    for (i = 0; i < len; i++)
    {
        spi_txrx(data[i]);
    }
    DW1000_CS_HIGH();
}

void dw_read_subreg(uint8_t reg_id, uint16_t sub_addr,
                     uint8_t *data, uint16_t len)
{
    uint16_t i;
    DW1000_CS_LOW();

    if (sub_addr <= 0x7FU)
    {
        spi_txrx(0x40U | (reg_id & 0x3FU));      /* bit7=0(read) bit6=1(sub-idx) */
        spi_txrx((uint8_t)(sub_addr & 0x7FU));
    }
    else
    {
        spi_txrx(0x40U | (reg_id & 0x3FU));
        spi_txrx((uint8_t)(0x80U | (sub_addr & 0x7FU)));
        spi_txrx((uint8_t)((sub_addr >> 7) & 0xFFU));
    }

    for (i = 0; i < len; i++)
    {
        data[i] = spi_txrx(0x00U);
    }
    DW1000_CS_HIGH();
}

/* ---- Convenience helpers (manual states LSB octet is sent/received first,
 *      "Note: The octets of a multi-octet value are transferred ... begin-
 *      ning with the low-order octet" - Figure 3 caption) -------------------*/
uint32_t dw_read_reg32(uint8_t reg_id)
{
    uint8_t b[4];
    dw_read_reg(reg_id, b, 4);
    return ((uint32_t)b[0]) | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

void dw_write_reg32(uint8_t reg_id, uint32_t value)
{
    uint8_t b[4];
    b[0] = (uint8_t)(value & 0xFFU);
    b[1] = (uint8_t)((value >> 8) & 0xFFU);
    b[2] = (uint8_t)((value >> 16) & 0xFFU);
    b[3] = (uint8_t)((value >> 24) & 0xFFU);
    dw_write_reg(reg_id, b, 4);
}

uint16_t dw_read_subreg16(uint8_t reg_id, uint16_t sub_addr)
{
    uint8_t b[2];
    dw_read_subreg(reg_id, sub_addr, b, 2);
    return ((uint16_t)b[0]) | ((uint16_t)b[1] << 8);
}

void dw_write_subreg16(uint8_t reg_id, uint16_t sub_addr, uint16_t value)
{
    uint8_t b[2];
    b[0] = (uint8_t)(value & 0xFFU);
    b[1] = (uint8_t)((value >> 8) & 0xFFU);
    dw_write_subreg(reg_id, sub_addr, b, 2);
}

void dw_write_subreg32(uint8_t reg_id, uint16_t sub_addr, uint32_t value)
{
    uint8_t b[4];
    b[0] = (uint8_t)(value & 0xFFU);
    b[1] = (uint8_t)((value >> 8) & 0xFFU);
    b[2] = (uint8_t)((value >> 16) & 0xFFU);
    b[3] = (uint8_t)((value >> 24) & 0xFFU);
    dw_write_subreg(reg_id, sub_addr, b, 4);
}

/* ===========================================================================
 *  Reset
 *  Per the DWM1004C datasheet (Table 3 notes): "The STM MCU PB1 controls the
 *  reset pin of the DW1000."
 * ===========================================================================
 */
void dw_reset(void)
{
    /* Configure PB1 as output push-pull, no pull, assert low, release */
    GPIOB->MODER  &= ~(3UL << (1 * 2));
    GPIOB->MODER  |=  (1UL << (1 * 2));
    GPIOB->OTYPER &= ~(1UL << 1);
    GPIOB->PUPDR  &= ~(3UL << (1 * 2));

    GPIOB->BRR = (1UL << 1);     /* RSTn = 0 (assert)  */
    LL_mDelay(2);
    GPIOB->BSRR = (1UL << 1);    /* RSTn = 1 (release) */
    LL_mDelay(5);                /* DW1000 boot time, Section 2.4 / Figure 9 */
}

/* ===========================================================================
 *  Init
 * ===========================================================================
 */
bool dw_init(void)
{
    uint32_t dev_id;
    uint8_t  pmsc_ctrl0[2];
    uint8_t  otp_ctrl[2];

    /* GPIOB clock must already be enabled where CS/SCK/MISO/MOSI live;
     * also enable it here defensively for the reset pin */
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN;

    dw_reset();

    /* 1. Confirm Device ID (Section 7.2.2) ---------------------------------*/
    dev_id = dw_read_reg32(DW_REG_DEV_ID);
    if (dev_id != DW_DEVICE_ID_EXPECTED)
    {
        return false;
    }

    /* 2. Default configuration fixups (Section 2.5.5) ----------------------
     * These correct sub-optimal POR defaults BEFORE the device is used.
     * Values are exactly as specified in the manual for channel 5,
     * 16 MHz PRF, which is the DW1000 default mode (mode 2).            */

    /* 2.5.5.1  AGC_TUNE1 -> 0x8870 */
    dw_write_subreg16(DW_REG_AGC_CTRL, DW_SUBADDR_AGC_TUNE1, 0x8870U);

    /* 2.5.5.2  AGC_TUNE2 -> 0x2502A907 (fixed value mandated by manual) */
    dw_write_subreg32(DW_REG_AGC_CTRL, DW_SUBADDR_AGC_TUNE2, 0x2502A907UL);

    /* 2.5.5.3  DRX_TUNE2 -> 0x311A002D */
    dw_write_subreg32(DW_REG_DRX_CONF, DW_SUBADDR_DRX_TUNE2, 0x311A002DUL);

    /* 2.5.5.4  NTM (LDE_CFG1) -> 0xD (lower 5 bits of that sub-register) */
    {
        uint8_t lde_cfg1;
        dw_read_subreg(DW_REG_LDE_CTRL, DW_SUBADDR_NTM, &lde_cfg1, 1);
        lde_cfg1 = (uint8_t)((lde_cfg1 & 0xE0U) | 0x0DU);
        dw_write_subreg(DW_REG_LDE_CTRL, DW_SUBADDR_NTM, &lde_cfg1, 1);
    }

    /* 2.5.5.5  LDE_CFG2 -> 0x1607 (for 16 MHz PRF) */
    dw_write_subreg16(DW_REG_LDE_CTRL, DW_SUBADDR_LDE_CFG2, 0x1607U);

    /* 2.5.5.6  TX_POWER -> 0x0E082848 */
    dw_write_reg32(DW_REG_TX_POWER, 0x0E082848UL);

    /* 2.5.5.7  RF_TXCTRL -> per Table 38 for channel 5 = 0x001E3FE3 */
    dw_write_subreg32(DW_REG_RF_CONF, DW_SUBADDR_RF_TXCTRL, 0x001E3FE3UL);

    /* 2.5.5.8  TC_PGDELAY -> 0xB5 for channel 5 */
    {
        uint8_t pgdelay = 0xB5U;
        dw_write_subreg(DW_REG_TX_CAL, DW_SUBADDR_TC_PGDELAY, &pgdelay, 1);
    }

    /* 2.5.5.9  FS_PLLTUNE -> 0xBE for channel 5 */
    {
        uint8_t plltune = 0xBEU;
        dw_write_subreg(DW_REG_FS_CTRL, DW_SUBADDR_FS_PLLTUNE, &plltune, 1);
    }

    /* 3. LDE microcode load (Table 4, Section 2.5.5.10) ---------------------
     * L-1: write PMSC_CTRL0 (0x36:00) = 0x0301
     * L-2: write OTP_CTRL   (0x2D:06) = 0x8000
     * wait 150 us
     * L-3: write PMSC_CTRL0 (0x36:00) = 0x0200
     */
    pmsc_ctrl0[0] = 0x01U; pmsc_ctrl0[1] = 0x03U;
    dw_write_subreg(DW_REG_PMSC, DW_SUBADDR_PMSC_CTRL0, pmsc_ctrl0, 2);

    otp_ctrl[0] = 0x00U; otp_ctrl[1] = 0x80U;
    dw_write_subreg(DW_REG_OTP_IF, DW_SUBADDR_OTP_CTRL, otp_ctrl, 2);

    /* 150 us delay -- LL_mDelay is 1ms resolution, round up to 1ms */
    LL_mDelay(1);

    pmsc_ctrl0[0] = 0x00U; pmsc_ctrl0[1] = 0x02U;
    dw_write_subreg(DW_REG_PMSC, DW_SUBADDR_PMSC_CTRL0, pmsc_ctrl0, 2);

    /* 4. Default antenna delay (placeholder -- MUST be calibrated, see
     *    notes at the bottom of this file / your build doc) */
    dw_set_tx_antenna_delay(16436U);
    dw_set_rx_antenna_delay(16436U);

    /* 5. Disable frame filtering (we want every frame for now, simplest
     *    for bring-up / debugging). Re-enable later for production. */
    {
        uint32_t sys_cfg = dw_read_reg32(DW_REG_SYS_CFG);
        sys_cfg &= ~DW_SYS_CFG_FFEN;
        dw_write_reg32(DW_REG_SYS_CFG, sys_cfg);
    }

    return true;
}

/* ===========================================================================
 *  Antenna delay
 *  Section 7.2.26 (TX_ANTD) / Sub-Register 0x2E:1804 (LDE_RXANTD)
 * ===========================================================================
 */
void dw_set_tx_antenna_delay(uint16_t delay_ticks)
{
    dw_write_reg32(DW_REG_TX_ANTD, 0); /* clear high bits first - reg is 2 bytes */
    dw_write_subreg16(DW_REG_TX_ANTD, 0, delay_ticks);
}

void dw_set_rx_antenna_delay(uint16_t delay_ticks)
{
    dw_write_subreg16(DW_REG_LDE_CTRL, DW_SUBADDR_LDE_RXANTD, delay_ticks);
}

/* ===========================================================================
 *  TX / RX frame primitives
 * ===========================================================================
 */
void dw_send_frame(const uint8_t *data, uint16_t len)
{
    uint32_t tx_fctrl;
    uint32_t status;

    /* Write payload to TX_BUFFER at offset 0 (Section 7.2.11) */
    dw_write_subreg(DW_REG_TX_BUFFER, 0, data, len);

    /* Configure TX_FCTRL: frame length = payload + 2 (FCS), 6.8Mbps,
     * 16MHz PRF, 128-symbol preamble  (Section 7.2.10) */
    tx_fctrl = (uint32_t)((len + 2U) & DW_TX_FCTRL_TFLEN_MASK)
             | DW_TX_FCTRL_TXBR_6M8
             | DW_TX_FCTRL_TXPRF_16M
             | DW_TX_FCTRL_TXPSR_128;
    dw_write_reg32(DW_REG_TX_FCTRL, tx_fctrl);

    /* Clear any stale TX status bits before starting (Section 7.2.17) */
    dw_write_reg32(DW_REG_SYS_STATUS, DW_SYS_STATUS_ALL_TX);

    /* Kick off transmission (Section 7.2.15, TXSTRT bit) */
    dw_write_reg32(DW_REG_SYS_CTRL, DW_SYS_CTRL_TXSTRT);

    /* Poll for TXFRS (Transmit Frame Sent) */
    do
    {
        status = dw_read_reg32(DW_REG_SYS_STATUS);
    } while ((status & DW_SYS_STATUS_TXFRS) == 0U);

    /* Clear TX status bits */
    dw_write_reg32(DW_REG_SYS_STATUS, DW_SYS_STATUS_ALL_TX);
}

int32_t dw_receive_frame(uint8_t *buf, uint16_t buf_len, uint32_t timeout_us)
{
    uint32_t status;
    uint32_t rx_finfo;
    uint16_t rx_len;
    uint32_t sys_cfg;

    /* Program RX frame wait timeout (Section 7.2.14). Units are ~1.026us. */
    if (timeout_us > 0U)
    {
        uint16_t fwto = (uint16_t)(timeout_us); /* approx 1us per count */
        dw_write_reg(DW_REG_RX_FWTO, (uint8_t *)&fwto, 2);

        sys_cfg = dw_read_reg32(DW_REG_SYS_CFG);
        sys_cfg |= DW_SYS_CFG_RXWTOE;
        dw_write_reg32(DW_REG_SYS_CFG, sys_cfg);
    }
    else
    {
        sys_cfg = dw_read_reg32(DW_REG_SYS_CFG);
        sys_cfg &= ~DW_SYS_CFG_RXWTOE;
        dw_write_reg32(DW_REG_SYS_CFG, sys_cfg);
    }

    /* Clear stale RX status bits */
    dw_write_reg32(DW_REG_SYS_STATUS,
                    DW_SYS_STATUS_ALL_RX_GOOD | DW_SYS_STATUS_ALL_RX_ERR);

    /* Enable receiver (Section 7.2.15, RXENAB bit). Note: 16us internal
     * delay before it actually starts listening for preamble. */
    dw_write_reg32(DW_REG_SYS_CTRL, DW_SYS_CTRL_RXENAB);

    /* Poll until frame ready, FCS error, or timeout */
    for (;;)
    {
        status = dw_read_reg32(DW_REG_SYS_STATUS);

        if (status & DW_SYS_STATUS_RXDFR)
        {
            /* Frame is in. Check FCS. */
            if (status & DW_SYS_STATUS_RXFCE)
            {
                /* Bad CRC -- clear and report error */
                dw_write_reg32(DW_REG_SYS_STATUS,
                                DW_SYS_STATUS_ALL_RX_GOOD | DW_SYS_STATUS_ALL_RX_ERR);
                return -1;
            }
            break; /* RXFCG should be set (good frame) */
        }

        if (status & (DW_SYS_STATUS_RXPHE | DW_SYS_STATUS_RXRFSL |
                      DW_SYS_STATUS_RXRFTO | DW_SYS_STATUS_RXSFDTO))
        {
            dw_write_reg32(DW_REG_SYS_STATUS,
                            DW_SYS_STATUS_ALL_RX_GOOD | DW_SYS_STATUS_ALL_RX_ERR);

            g_debug_status = status;

            return -1; /* error or timeout */
        }
    }

    /* Read RX_FINFO for frame length (Section 7.2.18, RXFLEN bits 6:0) */
    rx_finfo = dw_read_reg32(DW_REG_RX_FINFO);
    rx_len = (uint16_t)(rx_finfo & 0x7FU);

    if (rx_len > 2U)
    {
        rx_len -= 2U; /* drop the 2-byte FCS, manual recommends not reading it */
    }
    if (rx_len > buf_len)
    {
        rx_len = buf_len; /* avoid overflow of caller's buffer */
    }

    dw_read_subreg(DW_REG_RX_BUFFER, 0, buf, rx_len);

    /* Clear RX status bits */
    dw_write_reg32(DW_REG_SYS_STATUS,
                    DW_SYS_STATUS_ALL_RX_GOOD | DW_SYS_STATUS_ALL_RX_ERR);

    return (int32_t)rx_len;
}

/* ===========================================================================
 *  Timestamps
 *  TX_TIME (0x17): TX_STAMP is the low 40 bits, octets 0-4 (Section 7.2.25)
 *  RX_TIME (0x15): RX_STAMP is the low 40 bits, octets 0-4 (Section 7.2.23)
 * ===========================================================================
 */
dw_timestamp_t dw_read_tx_timestamp(void)
{
    uint8_t b[5];
    dw_read_subreg(DW_REG_TX_TIME, 0, b, 5);
    return ((dw_timestamp_t)b[0])       | ((dw_timestamp_t)b[1] << 8)  |
           ((dw_timestamp_t)b[2] << 16) | ((dw_timestamp_t)b[3] << 24) |
           ((dw_timestamp_t)b[4] << 32);
}

dw_timestamp_t dw_read_rx_timestamp(void)
{
    uint8_t b[5];
    dw_read_subreg(DW_REG_RX_TIME, 0, b, 5);
    return ((dw_timestamp_t)b[0])       | ((dw_timestamp_t)b[1] << 8)  |
           ((dw_timestamp_t)b[2] << 16) | ((dw_timestamp_t)b[3] << 24) |
           ((dw_timestamp_t)b[4] << 32);
}

/* ===========================================================================
 *  SS-TWR
 *
 *  Tprop = (Tround - Treply) / 2          [User Manual, Appendix 3, Fig. 36]
 *  Distance = Tprop * c                   (c = speed of light)
 *
 *  Tick period: 1 / (128 * 499.2e6) seconds ~= 1.5650040064e-11 s
 *  (Section 7.2.23 "the units of the low order bit are approximately
 *   15.65 picoseconds")
 * ===========================================================================
 */
#define DW_TICK_TO_SEC   (1.0f / (128.0f * 499.2e6f))
#define SPEED_OF_LIGHT_M_S  299702547.0f   /* in air, ~0.9997c */

/* Simple frame layout we use for the ranging exchange (not a strict IEEE
 * 802.15.4 MAC frame -- this is the simplest possible payload that still
 * works for a 2-board bring-up). Byte 0 = function code identifies the
 * message type. */
typedef struct __attribute__((packed))
{
    uint8_t func_code;             /* DW_TWR_FUNC_CODE_POLL */
} twr_poll_msg_t;

typedef struct __attribute__((packed))
{
    uint8_t  func_code;            /* DW_TWR_FUNC_CODE_RESP */
    uint8_t  rx_poll_ts[5];        /* responder's RX timestamp of the Poll (40-bit) */
    uint8_t  tx_resp_ts[5];        /* responder's TX timestamp of this Response (40-bit) */
} twr_resp_msg_t;

static void pack_ts(uint8_t *dst, dw_timestamp_t ts)
{
    dst[0] = (uint8_t)(ts & 0xFFU);
    dst[1] = (uint8_t)((ts >> 8) & 0xFFU);
    dst[2] = (uint8_t)((ts >> 16) & 0xFFU);
    dst[3] = (uint8_t)((ts >> 24) & 0xFFU);
    dst[4] = (uint8_t)((ts >> 32) & 0xFFU);
}

static dw_timestamp_t unpack_ts(const uint8_t *src)
{
    return ((dw_timestamp_t)src[0])       | ((dw_timestamp_t)src[1] << 8)  |
           ((dw_timestamp_t)src[2] << 16) | ((dw_timestamp_t)src[3] << 24) |
           ((dw_timestamp_t)src[4] << 32);
}

bool dw_twr_initiator_range(float *distance_m)
{
    twr_poll_msg_t poll_msg;
    twr_resp_msg_t resp_msg;
    dw_timestamp_t tx_poll_ts, rx_resp_ts;
    dw_timestamp_t rx_poll_ts_remote, tx_resp_ts_remote;
    int64_t t_round, t_reply;
    float tprop_ticks;
    float tprop_sec;

    /* Debug variables - watch these in the expressions window */


    /* 1. Send Poll */
    poll_msg.func_code = DW_TWR_FUNC_CODE_POLL;
    dw_send_frame((uint8_t *)&poll_msg, sizeof(poll_msg));
    tx_poll_ts = dw_read_tx_timestamp();
    UART_SendString(" ******** Packets are Sending *********** \r\n");
    /* DEBUG: measure how many DW1000 ticks elapse between Poll TX done
     * and the moment we actually enable the receiver.
     * If this value > RESPONSE_DELAY_TICKS on the Responder (32000000),
     * the Response already flew past before we were listening. */
    {
        uint8_t sys_time_bytes[5] = {0};
        uint64_t sys_time_now;
        dw_read_reg(DW_REG_SYS_TIME, sys_time_bytes, 5);
        sys_time_now =
            ((uint64_t)sys_time_bytes[0])        |
            ((uint64_t)sys_time_bytes[1] << 8)   |
            ((uint64_t)sys_time_bytes[2] << 16)  |
            ((uint64_t)sys_time_bytes[3] << 24)  |
            ((uint64_t)sys_time_bytes[4] << 32);
        ticks_elapsed = sys_time_now - tx_poll_ts;
        /* ticks_elapsed / 64000 = microseconds elapsed approx
         * (64000 ticks ~= 1us at the 64GHz system clock rate) */
    }

    /* 2. Wait for Response */
    rx_len_debug = dw_receive_frame((uint8_t *)&resp_msg,
                                     sizeof(resp_msg), 50000U);

    sys_status_after_rx = dw_read_reg32(DW_REG_SYS_STATUS);
    rx_finfo_debug      = dw_read_reg32(DW_REG_RX_FINFO);

    if (rx_len_debug != (int32_t)sizeof(resp_msg))
    {
        return false;   /* <<< PUT YOUR BREAKPOINT ON THIS LINE */
    }

    if (resp_msg.func_code != DW_TWR_FUNC_CODE_RESP)
    {
        return false;
    }
    rx_resp_ts = dw_read_rx_timestamp();

    /* 3. Extract responder's embedded timestamps */
    rx_poll_ts_remote = unpack_ts(resp_msg.rx_poll_ts);
    tx_resp_ts_remote = unpack_ts(resp_msg.tx_resp_ts);

    /* 4. Compute Tround / Treply */
    {
        const uint64_t MASK40 = ((uint64_t)1 << 40) - 1U;
        t_round = (int64_t)(((uint64_t)rx_resp_ts  - (uint64_t)tx_poll_ts)        & MASK40);
        t_reply = (int64_t)(((uint64_t)tx_resp_ts_remote - (uint64_t)rx_poll_ts_remote) & MASK40);
    }

    /* 5. Tprop = (Tround - Treply) / 2 */
    tprop_ticks = (float)(t_round - t_reply) / 2.0f;
    tprop_sec   = tprop_ticks * DW_TICK_TO_SEC;

    *distance_m = tprop_sec * SPEED_OF_LIGHT_M_S;

    return true;
}

void dw_twr_responder_listen(void)
{
    twr_poll_msg_t poll_msg;
    twr_resp_msg_t resp_msg;
    int32_t  rx_len;
    dw_timestamp_t rx_poll_ts;
    dw_timestamp_t planned_raw_tx_time;
    dw_timestamp_t planned_antenna_adjusted_tx_time;
    uint16_t tx_antd;
    uint8_t  dx_time[5];
    uint32_t status;
    uint32_t tx_fctrl;

    /* Delay (in system-time ticks) between RX of Poll and planned TX of the
     * Response. Must be long enough for this function to compute and load
     * the TX buffer before that time arrives. 100us is generous for
     * bring-up. (Section 3.3 -- Delayed Transmission) */
    const uint64_t RESPONSE_DELAY_TICKS = 50000000ULL; /* ~100 us */

    /* 1. Wait (blocking, no timeout) for a Poll message */
    rx_len = dw_receive_frame((uint8_t *)&poll_msg, sizeof(poll_msg), 0U);
    if (rx_len != (int32_t)sizeof(poll_msg))
    {
        return; /* bad frame, ignore and let caller loop again */
    }
    if (poll_msg.func_code != DW_TWR_FUNC_CODE_POLL)
    {
        return; /* not a Poll, ignore */
    }
    rx_poll_ts = dw_read_rx_timestamp();

    /* 2. Use DELAYED TRANSMISSION (Section 3.3) so the Response's actual TX
     *    timestamp is known in advance. This lets us embed the correct
     *    tx_resp_ts in the payload BEFORE transmitting it, which is
     *    otherwise impossible since a frame already on air cannot be
     *    edited.
     *
     *    Per Section 3.3: "the transmission time specified is the time of
     *    transmission of the RMARKER (not including the TX antenna
     *    delay), that is the raw TX time... before the antenna delay is
     *    added." The DW1000 automatically adds TX_ANTD on top of this to
     *    produce the actual TX_STAMP. So to embed the correct, antenna-
     *    adjusted timestamp that the Initiator will compare against, we
     *    add TX_ANTD ourselves before packing it into the payload. */
    planned_raw_tx_time = (rx_poll_ts + RESPONSE_DELAY_TICKS)
                           & ~((dw_timestamp_t)0x1FFU);  /* low 9 bits ignored */

    tx_antd = dw_read_subreg16(DW_REG_TX_ANTD, 0);
    planned_antenna_adjusted_tx_time = planned_raw_tx_time + (dw_timestamp_t)tx_antd;

    resp_msg.func_code = DW_TWR_FUNC_CODE_RESP;
    pack_ts(resp_msg.rx_poll_ts, rx_poll_ts);
    pack_ts(resp_msg.tx_resp_ts, planned_antenna_adjusted_tx_time);

    dw_write_subreg(DW_REG_TX_BUFFER, 0, (uint8_t *)&resp_msg, sizeof(resp_msg));

    tx_fctrl = (uint32_t)((sizeof(resp_msg) + 2U) & DW_TX_FCTRL_TFLEN_MASK)
             | DW_TX_FCTRL_TXBR_6M8
             | DW_TX_FCTRL_TXPRF_16M
             | DW_TX_FCTRL_TXPSR_128;
    dw_write_reg32(DW_REG_TX_FCTRL, tx_fctrl);

    /* Program DX_TIME (Section 7.2.12) with the RAW time (antenna delay
     * NOT included -- the DW1000 adds it automatically at TX) */
    dx_time[0] = (uint8_t)(planned_raw_tx_time & 0xFFU);
    dx_time[1] = (uint8_t)((planned_raw_tx_time >> 8) & 0xFFU);
    dx_time[2] = (uint8_t)((planned_raw_tx_time >> 16) & 0xFFU);
    dx_time[3] = (uint8_t)((planned_raw_tx_time >> 24) & 0xFFU);
    dx_time[4] = (uint8_t)((planned_raw_tx_time >> 32) & 0xFFU);
    dw_write_reg(DW_REG_DX_TIME, dx_time, 5);

    dw_write_reg32(DW_REG_SYS_STATUS, DW_SYS_STATUS_ALL_TX);

    /* TXSTRT + TXDLYS set together in the same write (Section 7.2.15,
     * TXDLYS field description: "both TXDLYS and TXSTRT should be set at
     * the same time") */
    dw_write_reg32(DW_REG_SYS_CTRL, DW_SYS_CTRL_TXSTRT | DW_SYS_CTRL_TXDLYS);

    /* Poll for TXFRS (Transmit Frame Sent) */
    do
    {
        status = dw_read_reg32(DW_REG_SYS_STATUS);
    } while ((status & DW_SYS_STATUS_TXFRS) == 0U);

    /* Check HPDWARN (Section 3.3): if set, the delayed TX was issued too
     * late and the actual TX time may not equal what we planned, which
     * would make our embedded tx_resp_ts wrong. For bring-up we simply
     * abort this exchange; the Initiator's receive will time out and it
     * can retry. If this happens often, increase RESPONSE_DELAY_TICKS. */
    if (status & DW_SYS_STATUS_HPDWARN)
    {
        dw_write_reg32(DW_REG_SYS_CTRL, DW_SYS_CTRL_TRXOFF);
    }

    dw_write_reg32(DW_REG_SYS_STATUS, DW_SYS_STATUS_ALL_TX);
}
