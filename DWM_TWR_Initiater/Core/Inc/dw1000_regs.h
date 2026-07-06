/******************************************************************************
 * dw1000_regs.h
 *
 * Register addresses and bit positions taken directly from the
 * DW1000 User Manual, Version 2.18 (Decawave Ltd 2017).
 *
 * Reference: Table 15, Register map overview, page 57-58.
 ******************************************************************************/
#ifndef DW1000_REGS_H
#define DW1000_REGS_H

#include <stdint.h>

/* ---------------- Register File IDs (top level, 6-bit) -------------------- */
#define DW_REG_DEV_ID        0x00U   /* RO  4  Device Identifier                 */
#define DW_REG_EUI           0x01U   /* RW  8  Extended Unique Identifier        */
#define DW_REG_PANADR        0x03U   /* RW  4  PAN Identifier and Short Address  */
#define DW_REG_SYS_CFG       0x04U   /* RW  4  System Configuration              */
#define DW_REG_SYS_TIME      0x06U   /* RO  5  System Time Counter (40-bit)      */
#define DW_REG_TX_FCTRL      0x08U   /* RW  5  Transmit Frame Control            */
#define DW_REG_TX_BUFFER     0x09U   /* WO  1024 Transmit Data Buffer            */
#define DW_REG_DX_TIME       0x0AU   /* RW  5  Delayed Send or Receive Time      */
#define DW_REG_RX_FWTO       0x0CU   /* RW  2  Receive Frame Wait Timeout Period */
#define DW_REG_SYS_CTRL      0x0DU   /* SRW 4  System Control Register           */
#define DW_REG_SYS_MASK      0x0EU   /* RW  4  System Event Mask Register        */
#define DW_REG_SYS_STATUS    0x0FU   /* SRW 5  System Event Status Register      */
#define DW_REG_RX_FINFO      0x10U   /* ROD 4  RX Frame Information              */
#define DW_REG_RX_BUFFER     0x11U   /* ROD 1024 Receive Data                    */
#define DW_REG_RX_FQUAL      0x12U   /* ROD 8  Rx Frame Quality information      */
#define DW_REG_RX_TIME       0x15U   /* ROD 14 Receive Message Time of Arrival   */
#define DW_REG_TX_TIME       0x17U   /* RO  10 Transmit Message Time of Sending  */
#define DW_REG_TX_ANTD       0x18U   /* RW  2  16-bit Delay from Transmit to Antenna */
#define DW_REG_SYS_STATE     0x19U   /* RO  5  System State information          */
#define DW_REG_ACK_RESP_T    0x1AU   /* RW  4  Acknowledgement/Response Time     */
#define DW_REG_TX_POWER      0x1EU   /* RW  4  TX Power Control                  */
#define DW_REG_CHAN_CTRL     0x1FU   /* RW  4  Channel Control                   */
#define DW_REG_AGC_CTRL      0x23U   /* RW  33 AGC configuration                 */
#define DW_REG_GPIO_CTRL     0x26U   /* RW  44 GPIO control                      */
#define DW_REG_DRX_CONF      0x27U   /* RW  44 Digital Receiver configuration    */
#define DW_REG_RF_CONF       0x28U   /* RW  58 Analog RF Configuration           */
#define DW_REG_TX_CAL        0x2AU   /* RW  52 Transmitter calibration block     */
#define DW_REG_FS_CTRL       0x2BU   /* RW  21 Frequency synthesiser control     */
#define DW_REG_AON           0x2CU   /* RW  12 Always-On register set            */
#define DW_REG_OTP_IF        0x2DU   /* RW  18 OTP Memory Interface              */
#define DW_REG_LDE_CTRL      0x2EU   /* RW  -- Leading edge detection control    */
#define DW_REG_DIG_DIAG      0x2FU   /* RW  41 Digital Diagnostics Interface     */
#define DW_REG_PMSC          0x36U   /* RW  48 Power Management System Control   */

/* ---------------- Sub-register / sub-index addresses used in this driver --- */
#define DW_SUBADDR_AGC_TUNE1    0x04U   /* Sub-Register 0x23:04 */
#define DW_SUBADDR_AGC_TUNE2    0x0CU   /* Sub-Register 0x23:0C */
#define DW_SUBADDR_DRX_TUNE2    0x08U   /* Sub-Register 0x27:08 */
#define DW_SUBADDR_NTM          0x0806U /* Sub-Register 0x2E:0806 - LDE_CFG1 */
#define DW_SUBADDR_LDE_CFG2     0x1806U /* Sub-Register 0x2E:1806 - LDE_CFG2 */
#define DW_SUBADDR_LDE_RXANTD   0x1804U /* Sub-Register 0x2E:1804 - LDE_RXANTD */
#define DW_SUBADDR_RF_TXCTRL    0x0CU   /* Sub-Register 0x28:0C - RF_TXCTRL */
#define DW_SUBADDR_TC_PGDELAY   0x0BU   /* Sub-Register 0x2A:0B - TC_PGDELAY */
#define DW_SUBADDR_FS_PLLTUNE   0x0BU   /* Sub-Register 0x2B:0B - FS_PLLTUNE */
#define DW_SUBADDR_PMSC_CTRL0   0x00U   /* Sub-Register 0x36:00 */
#define DW_SUBADDR_PMSC_CTRL1   0x04U   /* Sub-Register 0x36:04 */
#define DW_SUBADDR_OTP_CTRL     0x06U   /* Sub-Register 0x2D:06 */

/* ---------------- SYS_CTRL (0x0D) bit positions ---------------------------
 * Reference: Section 7.2.15, REG:0D:00 bit layout, page 74-77
 *--------------------------------------------------------------------------*/
#define DW_SYS_CTRL_SFCST       (1UL << 0)   /* Suppress auto-FCS Transmission */
#define DW_SYS_CTRL_TXSTRT      (1UL << 1)   /* Transmit Start */
#define DW_SYS_CTRL_TXDLYS      (1UL << 2)   /* Transmitter Delayed Sending */
#define DW_SYS_CTRL_CANSFCS     (1UL << 3)   /* Cancel Suppression of auto-FCS */
#define DW_SYS_CTRL_TRXOFF      (1UL << 6)   /* Transceiver Off */
#define DW_SYS_CTRL_WAIT4RESP   (1UL << 7)   /* Wait for Response */
#define DW_SYS_CTRL_RXENAB      (1UL << 8)   /* Enable Receiver */
#define DW_SYS_CTRL_RXDLYE      (1UL << 9)   /* Receiver Delayed Enable */
#define DW_SYS_CTRL_HRBPT       (1UL << 24)  /* Host Side RX Buffer Pointer Toggle */

/* ---------------- SYS_STATUS (0x0F) bit positions --------------------------
 * Reference: Section 7.2.17, REG:0F:00 / 0F:04 bit layout, page 80-87
 *--------------------------------------------------------------------------*/
#define DW_SYS_STATUS_IRQS      (1UL << 0)
#define DW_SYS_STATUS_CPLOCK    (1UL << 1)
#define DW_SYS_STATUS_ESYNCR    (1UL << 2)
#define DW_SYS_STATUS_AAT       (1UL << 3)
#define DW_SYS_STATUS_TXFRB     (1UL << 4)   /* Transmit Frame Begins */
#define DW_SYS_STATUS_TXPRS     (1UL << 5)   /* Transmit Preamble Sent */
#define DW_SYS_STATUS_TXPHS     (1UL << 6)   /* Transmit PHY Header Sent */
#define DW_SYS_STATUS_TXFRS     (1UL << 7)   /* Transmit Frame Sent  <-- TX done flag */
#define DW_SYS_STATUS_RXPRD     (1UL << 8)   /* Receiver Preamble Detected */
#define DW_SYS_STATUS_RXSFDD    (1UL << 9)   /* Receiver SFD Detected */
#define DW_SYS_STATUS_LDEDONE   (1UL << 10)  /* LDE processing done */
#define DW_SYS_STATUS_RXPHD     (1UL << 11)  /* Receiver PHY Header Detect */
#define DW_SYS_STATUS_RXPHE     (1UL << 12)  /* Receiver PHY Header Error */
#define DW_SYS_STATUS_RXDFR     (1UL << 13)  /* Receiver Data Frame Ready <-- RX done flag */
#define DW_SYS_STATUS_RXFCG     (1UL << 14)  /* Receiver FCS Good */
#define DW_SYS_STATUS_RXFCE     (1UL << 15)  /* Receiver FCS Error */
#define DW_SYS_STATUS_RXRFSL    (1UL << 16)  /* Receiver Reed-Solomon Frame Sync Loss */
#define DW_SYS_STATUS_RXRFTO    (1UL << 17)  /* Receive Frame Wait Timeout */
#define DW_SYS_STATUS_LDEERR    (1UL << 18)
#define DW_SYS_STATUS_RXOVRR    (1UL << 20)
#define DW_SYS_STATUS_RXPTO     (1UL << 21)
#define DW_SYS_STATUS_RXSFDTO   (1UL << 26)
#define DW_SYS_STATUS_HPDWARN   (1UL << 27)
#define DW_SYS_STATUS_TXBERR    (1UL << 28)
#define DW_SYS_STATUS_AFFREJ    (1UL << 29)

/* All "frame done" or error bits we clear after a TX / RX cycle */
#define DW_SYS_STATUS_ALL_TX  (DW_SYS_STATUS_TXFRB | DW_SYS_STATUS_TXPRS | \
                                DW_SYS_STATUS_TXPHS | DW_SYS_STATUS_TXFRS)

#define DW_SYS_STATUS_ALL_RX_GOOD (DW_SYS_STATUS_RXPRD  | DW_SYS_STATUS_RXSFDD | \
                                    DW_SYS_STATUS_LDEDONE| DW_SYS_STATUS_RXPHD  | \
                                    DW_SYS_STATUS_RXDFR  | DW_SYS_STATUS_RXFCG)

#define DW_SYS_STATUS_ALL_RX_ERR  (DW_SYS_STATUS_RXPHE | DW_SYS_STATUS_RXFCE | \
                                    DW_SYS_STATUS_RXRFSL| DW_SYS_STATUS_RXRFTO| \
                                    DW_SYS_STATUS_RXSFDTO)

/* ---------------- SYS_CFG (0x04) bit positions ----------------------------
 * Reference: Section 7.2.6, page 65-67
 *--------------------------------------------------------------------------*/
#define DW_SYS_CFG_FFEN       (1UL << 0)   /* Frame Filtering Enable */
#define DW_SYS_CFG_RXAUTR     (1UL << 29)  /* Receiver Auto Re-enable */
#define DW_SYS_CFG_HIRQ_POL   (1UL << 9)   /* IRQ polarity */
#define DW_SYS_CFG_DIS_STXP   (1UL << 18)  /* Disable Smart TX power */
#define DW_SYS_CFG_RXWTOE     (1UL << 28)  /* Receive Wait Timeout Enable */

/* ---------------- TX_FCTRL (0x08) field shifts -----------------------------
 * Reference: Section 7.2.10, page 68-71
 *--------------------------------------------------------------------------*/
#define DW_TX_FCTRL_TFLEN_MASK    (0x7FUL)        /* bits 6:0 */
#define DW_TX_FCTRL_TXBR_SHIFT    13              /* bits 14:13 */
#define DW_TX_FCTRL_TXBR_6M8      (0x2UL << DW_TX_FCTRL_TXBR_SHIFT)
#define DW_TX_FCTRL_TXPRF_SHIFT   16              /* bits 17:16 */
#define DW_TX_FCTRL_TXPRF_16M     (0x1UL << DW_TX_FCTRL_TXPRF_SHIFT)
#define DW_TX_FCTRL_TXPSR_SHIFT   18              /* bits 19:18 */
#define DW_TX_FCTRL_TXPSR_128     (0x1UL << DW_TX_FCTRL_TXPSR_SHIFT)

/* DEV ID expected value, Section 7.2.2, page 60 */
#define DW_DEVICE_ID_EXPECTED   0xDECA0130UL

#endif /* DW1000_REGS_H */
