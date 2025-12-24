# Robot Firmware Documentation (TM4C123)

[< Back to Project Root](../README.md)

## Overview

This C program runs on the **TM4C123GH6PM** microcontroller. It handles low-level hardware control, including PWM generation for the servos and UART interrupts for receiving commands. It uses **Direct Register Access (DRA)** rather than libraries for maximum performance and educational value.

## Register Command Reference

| Register             | Purpose        | Value Used   | Description                                         |
| :------------------- | :------------- | :----------- | :-------------------------------------------------- |
| `SYSCTL_RCGC_PWM_R`  | Clock Gating   | `0x01`       | Enables PWM Module 0.                               |
| `SYSCTL_RCGC_GPIO_R` | Clock Gating   | `0x16`       | Enables Ports B (Base, Shldr), C (Claw), E (Elbow). |
| `SYSCTL_RCC_R`       | System Control | `0x000A0000` | Sets PWM Divider to /64.                            |
| `PWM0_X_GENA_R`      | PWM Generator  | `0x8C`       | Drive High on Load, Low on Comparator A.            |
| `PWM0_X_LOAD_R`      | PWM Period     | `4999`       | Sets 50Hz frequency (20ms) for Servos.              |
| `UART1_IBRD_R`       | UART Baud      | `8`          | Integer part of 115200 Baud.                        |
| `UART1_LCRH_R`       | UART Line Ctrl | `0x60`       | 8-bit, No Parity, 1 Stop Bit (8N1).                 |
| `NVIC_EN0_R`         | Interrupts     | `0x40`       | Enables UART1 Interrupt (IRQ 6).                    |

## Code Walkthrough

### 1. System Control & Clock Gating

```c
SYSCTL_RCGC_PWM_R |= 0x01;  // Enable PWM Module 0
SYSCTL_RCGC_GPIO_R |= 0x16; // Enable Ports B, C, E
SYSCTL_RCGC_UART_R |= 0x02; // Enable UART Module 1
```

- **`SYSCTL_RCGC_...`**: These registers enable the system clock for specific peripherals. Without this, attempting to access a peripheral will cause a generic fault.

### 2. PWM Configuration (Servo Control)

4 Servos are controlled using **PWM0**.

- **Base**: PB6 (Generator 0)
- **Shoulder**: PB4 (Generator 1)
- **Elbow**: PE4 (Generator 2)
- **Claw**: PC4 (Generator 3)

### 3. UART Configuration (Communication)

UART1 is used to receive data from the Computer or Receiver ESP32.

- **`UART1_IM_R |= 0x10`**: Enables the **Receive Interrupt (RXIM)**. This ensures the CPU is only notified when data arrives, saving processing power.
- **`NVIC_EN0_R |= 0x40`**: Enables Interrupt #6 (UART1) in the Nested Vectored Interrupt Controller (NVIC).

### 4. Interrupt Handler (`UART1_Handler`)

This function runs automatically whenever a byte is received.

- **State Machine**:
  - It waits for `HEADER_BYTE` (`0xFF`).
  - Then records 4 bytes (Base, Shoulder, Elbow, Claw).
  - Finally waits for `FOOTER_BYTE` (`0xFE`).
  - If valid, it calls `Move_Servos()`.
