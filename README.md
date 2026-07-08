
STM32 + DW1000 UWB Distance Ranging
A bare-metal firmware project for measuring real-world distance between two DW1000 UWB modules using an STM32L041 microcontroller. No HAL. No RTOS. Just registers, SPI, and math.
---
What This Does
This project measures the physical distance between two devices — accurate to within 10 cm — using Ultra-Wideband radio. One device acts as the Initiator (Tag) and the other as the Responder (Anchor). They exchange a pair of UWB packets, record precise hardware timestamps at each step, and the Initiator uses those four timestamps to calculate how long the radio wave spent in the air.
```
Initiator (Tag)              Responder (Anchor)
STM32 + DW1000  <--------->  STM32 + DW1000
```
Convert that flight time to meters and you have your distance.
---
Hardware
Component	Part
Module	DWM1004C (DW1000 + STM32L041G6U6S on one board)
MCU	STM32L041G6U6S — Cortex-M0+, 32kB Flash, 8kB RAM
UWB Transceiver	Decawave DW1000
Interface	SPI1 (bare-metal LL driver)
Pin Connections
Signal	STM32 Pin
SPI SCK	PB3
SPI MISO	PB4
SPI MOSI	PB5
CS (Chip Select)	PA4
The DW1000 sits on the same module as the STM32 and is wired internally. The STM32 communicates with it entirely over SPI.
---
How It Works
Step 1 — Power on and initialize
When the board boots, the STM32 initializes the system clock, sets up SPI1, and configures the DW1000 by writing its registers over SPI. This sets the UWB channel, data rate, preamble length, TX power, and antenna delay.
Step 2 — Initiator sends a POLL packet
The Initiator builds a small UWB frame and writes it into the DW1000's transmit buffer. It then tells the DW1000 to start transmitting. From this point, the DW1000 hardware takes over — the STM32 is not involved in generating the radio signal.
The moment the packet goes out, the DW1000 automatically captures the exact transmission time in its internal timestamp register.
```
T1 = Poll transmission time  (recorded by Initiator's DW1000)
```
Step 3 — Responder receives the POLL
The Responder's DW1000 is continuously listening. When the poll arrives, it captures the reception time in hardware.
```
T2 = Poll reception time  (recorded by Responder's DW1000)
```
Step 4 — Responder sends a RESPONSE
After a short, known processing delay, the Responder sends a reply packet. The DW1000 records the exact moment it goes out.
```
T3 = Response transmission time  (recorded by Responder's DW1000)
```
The response packet carries T2 and T3 so the Initiator can use them.
Step 5 — Initiator receives the RESPONSE
The Initiator's DW1000 captures the moment the response arrives.
```
T4 = Response reception time  (recorded by Initiator's DW1000)
```
Step 6 — Calculate distance
Now the Initiator has all four timestamps. The math removes the time the Responder spent processing and leaves only the time the radio signal spent traveling through the air.
```
Round Trip Time        = T4 - T1
Responder Turnaround   = T3 - T2
Time of Flight         = (Round Trip Time - Responder Turnaround) / 2
Distance               = Time of Flight × Speed of Light
```
For example, a Time of Flight of 33.4 ns gives:
```
33.4 × 10⁻⁹ s × 299,792,458 m/s ≈ 10 metres
```
---
Who Does What
The STM32 and the DW1000 have very different jobs in this system.
The STM32 handles control and math:
Initializes everything at startup
Sends configuration commands to the DW1000 over SPI
Builds the POLL and RESPONSE packet payloads
Reads the four timestamps back from DW1000 registers
Applies the SS-TWR formula and outputs the distance
The DW1000 handles radio and timing:
Generates and transmits the actual UWB pulses
Detects and receives incoming UWB frames
Captures TX and RX timestamps in hardware with picosecond resolution
Stores everything in registers for the STM32 to read
The centimeter-level accuracy comes from the DW1000's hardware timestamping. The STM32 couldn't achieve this on its own — software timing would have far too much jitter.
---
Project Structure
```
Core/
├── Inc/
│   ├── main.h
│   ├── spi.h               # SPI1 driver interface
│   ├── dwm1000.h           # DW1000 driver interface
│   ├── dwm1000\_registers.h # Full DW1000 register map
│   └── delay.h             # Millisecond delay interface
└── Src/
    ├── main.c              # Entry point, top-level sequencing
    ├── spi.c               # Bare-metal SPI1 driver
    ├── dwm1000.c           # DW1000 read/write + init
    └── delay.c             # SysTick-based delay
```
---
SPI Configuration
Parameter	Value
Mode	Master
Clock Mode	Mode 0 (CPOL=0, CPHA=0)
Data Size	8-bit
Bit Order	MSB first
Initial Speed	~8 kHz (safe for DW1000 startup)
Operating Speed	~1 MHz (after init)
The driver uses software-controlled chip select on PA4. The DW1000 SPI header format follows the Decawave specification — one byte for the register address (with R/W and sub-index flags in the upper bits), optionally followed by one or two sub-address bytes.
---
Ranging Method
This project uses Single-Sided Two-Way Ranging (SS-TWR). It requires only one round-trip exchange between two devices and gives good accuracy without needing synchronized clocks. The alternative, DS-TWR, uses a second round trip to cancel clock drift — that can be added later if higher accuracy is needed.
---
Status
[x] SPI1 bare-metal driver
[x] DW1000 register read/write
[x] Device ID verification
[ ] Full DW1000 configuration (channel, PRF, data rate)
[ ] POLL packet TX
[ ] RESPONSE packet RX
[ ] SS-TWR timestamp calculation
[ ] Distance output over UART
---
References
DW1000 User Manual v2.18 — Decawave
DWM1004C Datasheet v1.4 — Decawave/Qorvo
STM32L041G6 Reference Manual — STMicroelectronics
