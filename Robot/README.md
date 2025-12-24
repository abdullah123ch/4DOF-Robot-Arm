# Robot Firmware Documentation (TM4C123)

## Overview

This C program runs on the **TM4C123GH6PM** microcontroller. It handles low-level hardware control, including PWM generation for the servos and UART interrupts for receiving commands. It uses **Direct Register Access (DRA)** rather than libraries for maximum performance and educational value.

## Code Walkthrough & Register Explanation

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

**Key Commands:**

- **`SYSCTL_RCC_R |= 0x00100000`**: Enables the PWM clock divider.
- **`SYSCTL_RCC_R |= 0x000A0000`**: Sets divider to /64.
  - Calculation: 16MHz / 64 = 250kHz PWM Clock.
- **`PWM0_X_GENA_R = 0x8C`**: Configures the PWM output mode.
  - `0x8C`: Drive High when counter reloads, Drive Low when counter matches Comparator A. This creates the standard servo pulse.
- **`PWM0_X_LOAD_R = 4999`**: Sets the period.
  - 250kHz / 50Hz = 5000 ticks. We use 4999 (N-1).

### 3. UART Configuration (Communication)

UART1 is used to receive data from the Computer or Receiver ESP32.

- **`UART1_IBRD_R = 8; UART1_FBRD_R = 44;`**: Sets Baud Rate to 115200.
- **`UART1_LCRH_R = 0x60`**: Sets line control to 8 bits, no parity, 1 stop bit (8N1).
- **`UART1_IM_R |= 0x10`**: Enables the **Receive Interrupt (RXIM)**. This ensures the CPU is only notified when data arrives, saving processing power.
- **`NVIC_EN0_R |= 0x40`**: Enables Interrupt #6 (UART1) in the Nested Vectored Interrupt Controller (NVIC).

### 4. Interrupt Handler (`UART1_Handler`)

This function runs automatically whenever a byte is received.

- **`UART1_ICR_R = 0x10`**: Acknowledge/Clear the interrupt flag.
- **`UART1_DR_R`**: Reads the received data byte.
- **State Machine**:
  - It waits for `HEADER_BYTE` (`0xFF`).
  - Then records 4 bytes (Base, Shoulder, Elbow, Claw).
  - Finally waits for `FOOTER_BYTE` (`0xFE`).
  - If valid, it calls `Move_Servos()`.

### 5. Helper Function (`Map_Angle_To_Duty`)

```c
pulse_ticks = MIN_SERVO_TICKS + ((angle * (MAX_SERVO_TICKS - MIN_SERVO_TICKS)) / 180);
```

- Translates a human-readable angle (0-180) into the machine-specific PWM comparator value.
- **`MIN_SERVO_TICKS`** (150) corresponds to ~0.6ms pulse (0 degrees).
- **`MAX_SERVO_TICKS`** (600) corresponds to ~2.4ms pulse (180 degrees).
