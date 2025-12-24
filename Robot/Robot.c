/******************************************************************************
* Project: 4-DOF Robot Arm Controller (Receiver)
* Target: TM4C123GH6PM Tiva C
* Input: UART1 (PB0) from ESP32 @ 115200 Baud
* Output: PWM0 Servos -> Base(PB6), Shoulder(PB4), Elbow(PE4), Claw(PC4)
* Author: MPS Wale Bhai
******************************************************************************/

#include <stdint.h>
#include "TM4C123.h"

// ============================================================================
// 1. REGISTER DEFINITIONS
// ============================================================================

// --- System Control ---
#define SYSCTL_RCGC_PWM_R       (*((volatile uint32_t *)0x400FE640))
#define SYSCTL_RCGC_GPIO_R      (*((volatile uint32_t *)0x400FE608))
#define SYSCTL_RCGC_UART_R      (*((volatile uint32_t *)0x400FE618))
#define SYSCTL_RCC_R            (*((volatile uint32_t *)0x400FE060))

// --- GPIO Ports ---
// Port B (UART1 Rx=PB0, Base=PB6, Shoulder=PB4)
#define GPIO_PORTB_AFSEL_R      (*((volatile uint32_t *)0x40005420))
#define GPIO_PORTB_PCTL_R       (*((volatile uint32_t *)0x4000552C))
#define GPIO_PORTB_DEN_R        (*((volatile uint32_t *)0x4000551C))

// Port E (Elbow=PE4)
#define GPIO_PORTE_AFSEL_R      (*((volatile uint32_t *)0x40024420))
#define GPIO_PORTE_PCTL_R       (*((volatile uint32_t *)0x4002452C))
#define GPIO_PORTE_DEN_R        (*((volatile uint32_t *)0x4002451C))

// Port C (Claw=PC4)
#define GPIO_PORTC_AFSEL_R      (*((volatile uint32_t *)0x40006420))
#define GPIO_PORTC_PCTL_R       (*((volatile uint32_t *)0x4000652C))
#define GPIO_PORTC_DEN_R        (*((volatile uint32_t *)0x4000651C))

// --- UART1 (PB0 Rx) ---
#define UART1_CTL_R             (*((volatile uint32_t *)0x4000D030))
#define UART1_IBRD_R            (*((volatile uint32_t *)0x4000D024))
#define UART1_FBRD_R            (*((volatile uint32_t *)0x4000D028))
#define UART1_LCRH_R            (*((volatile uint32_t *)0x4000D02C))
#define UART1_IM_R              (*((volatile uint32_t *)0x4000D038))
#define UART1_ICR_R             (*((volatile uint32_t *)0x4000D044))
#define UART1_DR_R              (*((volatile uint32_t *)0x4000D000))
#define NVIC_EN0_R              (*((volatile uint32_t *)0xE000E100))

// --- PWM0 ---
#define PWM0_CTL_R              (*((volatile uint32_t *)0x40028000))
#define PWM0_SYNC_R             (*((volatile uint32_t *)0x40028004))
#define PWM0_ENABLE_R           (*((volatile uint32_t *)0x40028008))

// Generator 0 (PB6 - Base)
#define PWM0_0_CTL_R            (*((volatile uint32_t *)0x40028040))
#define PWM0_0_LOAD_R           (*((volatile uint32_t *)0x40028050))
#define PWM0_0_CMPA_R           (*((volatile uint32_t *)0x40028058))
#define PWM0_0_GENA_R           (*((volatile uint32_t *)0x40028060))

// Generator 1 (PB4 - Shoulder)
#define PWM0_1_CTL_R            (*((volatile uint32_t *)0x40028080))
#define PWM0_1_LOAD_R           (*((volatile uint32_t *)0x40028090))
#define PWM0_1_CMPA_R           (*((volatile uint32_t *)0x40028098))
#define PWM0_1_GENA_R           (*((volatile uint32_t *)0x400280A0))

// Generator 2 (PE4 - Elbow)
#define PWM0_2_CTL_R            (*((volatile uint32_t *)0x400280C0))
#define PWM0_2_LOAD_R           (*((volatile uint32_t *)0x400280D0))
#define PWM0_2_CMPA_R           (*((volatile uint32_t *)0x400280D8))
#define PWM0_2_GENA_R           (*((volatile uint32_t *)0x400280E0))

// Generator 3 (PC4 - Claw)
#define PWM0_3_CTL_R            (*((volatile uint32_t *)0x40028100))
#define PWM0_3_LOAD_R           (*((volatile uint32_t *)0x40028110))
#define PWM0_3_CMPA_R           (*((volatile uint32_t *)0x40028118))
#define PWM0_3_GENA_R           (*((volatile uint32_t *)0x40028120))

// ============================================================================
// 2. CONSTANTS
// ============================================================================
// Clock = 16MHz, PWM Div = 64 => PWM Clock = 250kHz (4us per tick)
// Period = 50Hz (20ms) => 250,000 / 50 = 5000 ticks
#define PWM_LOAD_VALUE      4999    // N-1

// Servo Calibration
#define MIN_SERVO_TICKS     150
#define MAX_SERVO_TICKS     600

// UART Protocol
#define HEADER_BYTE         0xFF
#define FOOTER_BYTE         0xFE

// ============================================================================
// 3. GLOBAL VARIABLES
// ============================================================================
volatile uint8_t rx_state = 0;      // FSM State
volatile uint8_t rx_buffer[4];      // Temp Data Storage
volatile uint8_t rx_index = 0;      // Index for buffer

// Default Angles (Center Position)
volatile uint8_t angle_base = 90;
volatile uint8_t angle_shoulder = 90;
volatile uint8_t angle_elbow = 90;
volatile uint8_t angle_claw = 0;

//System Initialization for Floating Point Unit
void SystemInit (void)
{
    /* --------------------------FPU settings ----------------------------------*/
    #if (__FPU_USED == 1)
        SCB->CPACR |= ((3UL << 10*2) |                /* set CP10 Full Access */
                  (3UL << 11*2)  );                /* set CP11 Full Access */
    #endif
}



// ============================================================================
// 4. FUNCTIONS
// ============================================================================

// --- Helper: Map Angle (0-180) to PWM CMPA Value ---
uint32_t Map_Angle_To_Duty(uint8_t angle) {
    uint32_t pulse_ticks; // FIXED: Variable declared at top

    // Safety Clamp
    if (angle > 180) angle = 180;
    
    // Calculate Pulse Width in Ticks
    pulse_ticks = MIN_SERVO_TICKS + ((angle * (MAX_SERVO_TICKS - MIN_SERVO_TICKS)) / 180);
    
    // Because we use Down-Count logic (0x8C):
    // High Time = LOAD - CMPA => CMPA = LOAD - High Time
    return (PWM_LOAD_VALUE - pulse_ticks);
}

// --- Action: Update Servos ---
void Move_Servos(void) {
    // 1. Update Buffered Registers (Motors won't move yet)
    PWM0_0_CMPA_R = Map_Angle_To_Duty(angle_base);      // PB6
    PWM0_1_CMPA_R = Map_Angle_To_Duty(angle_shoulder);  // PB4
    PWM0_2_CMPA_R = Map_Angle_To_Duty(angle_elbow);     // PE4
    PWM0_3_CMPA_R = Map_Angle_To_Duty(angle_claw);      // PC4
    
    // 2. Trigger Sync (Fire all changes at once!)
    // Sync Gen0(Bit0), Gen1(Bit1), Gen2(Bit2), Gen3(Bit3) -> 0x0F
    PWM0_SYNC_R = 0x0F; 
}

// --- Init: UART1 (115200 Baud) ---
void UART1_Init(void) {
    // 1. Enable Clocks
    SYSCTL_RCGC_UART_R |= 0x02;     // UART1
    SYSCTL_RCGC_GPIO_R |= 0x02;     // Port B
    
    // 2. GPIO Config (PB0 = U1Rx)
    GPIO_PORTB_AFSEL_R |= 0x01;     // Alt Function on PB0
    GPIO_PORTB_PCTL_R  &= ~0x0000000F; 
    GPIO_PORTB_PCTL_R  |=  0x00000001; // U1Rx
    GPIO_PORTB_DEN_R   |= 0x01;     // Digital Enable PB0
    
    // 3. UART Config
    UART1_CTL_R &= ~0x0001;         // Disable UART
    
    // Baud Rate: 16MHz / (16 * 115200) = 8.6805
    UART1_IBRD_R = 8;               // Integer part
    UART1_FBRD_R = 44;              // Fraction: 0.6805 * 64 + 0.5 = 44
    
    UART1_LCRH_R = 0x0060;          // 8-bit, no parity, 1-stop
    UART1_IM_R   |= 0x0010;         // Enable RX Interrupt (RXIM)
    
    UART1_CTL_R  |= 0x0301;         // Enable UART, Rx, Tx (0x200|0x100|0x01)
    
    // 4. NVIC Config
    // UART1 is Interrupt #6. Priority defaults to 0 (Highest).
    NVIC_EN0_R |= 0x00000040;       // Enable IRQ 6 (Bit 6)
}

// --- Init: PWM (50Hz, 4 Servos) ---
void PWM_Init(void) {
    volatile uint32_t delay;
    uint32_t center; // FIXED: Variable declared at top

    // 1. Enable Clocks
    SYSCTL_RCGC_PWM_R  |= 0x01;     // Module 0
    SYSCTL_RCGC_GPIO_R |= 0x16;     // Port B(0x02), C(0x04), E(0x10) -> 0x16
    delay = SYSCTL_RCGC_GPIO_R;     // Wait for clock
    
    // 2. Configure PWM Divider (/64)
    SYSCTL_RCC_R |= 0x00100000;     // USEPWMDIV
    SYSCTL_RCC_R &= ~0x000E0000;    // Clear div bits
    SYSCTL_RCC_R |= 0x000A0000;     // Set Div /64 (0x5)
    
    // 3. GPIO AFSEL & PCTL Setup
    // PB6 (M0PWM0)
    GPIO_PORTB_AFSEL_R |= 0x40;
    GPIO_PORTB_PCTL_R  &= ~0x0F000000;
    GPIO_PORTB_PCTL_R  |=  0x04000000; // 4
    GPIO_PORTB_DEN_R   |= 0x40;
    
    // PB4 (M0PWM2)
    GPIO_PORTB_AFSEL_R |= 0x10;
    GPIO_PORTB_PCTL_R  &= ~0x000F0000;
    GPIO_PORTB_PCTL_R  |=  0x00040000; // 4
    GPIO_PORTB_DEN_R   |= 0x10;
    
    // PE4 (M0PWM4)
    GPIO_PORTE_AFSEL_R |= 0x10;
    GPIO_PORTE_PCTL_R  &= ~0x000F0000;
    GPIO_PORTE_PCTL_R  |=  0x00040000; // 4
    GPIO_PORTE_DEN_R   |= 0x10;
    
    // PC4 (M0PWM6)
    GPIO_PORTC_AFSEL_R |= 0x10;
    GPIO_PORTC_PCTL_R  &= ~0x000F0000;
    GPIO_PORTC_PCTL_R  |=  0x00040000; // 4
    GPIO_PORTC_DEN_R   |= 0x10;
    
    // 4. Configure Generators (0, 1, 2, 3)
    // Disable first
    PWM0_0_CTL_R = 0; PWM0_1_CTL_R = 0; PWM0_2_CTL_R = 0; PWM0_3_CTL_R = 0;
    
    // Logic 0x8C: Drive High on Load, Drive Low on CMPA Down
    PWM0_0_GENA_R = 0x8C;
    PWM0_1_GENA_R = 0x8C;
    PWM0_2_GENA_R = 0x8C;
    PWM0_3_GENA_R = 0x8C;
    
    // Period (50Hz)
    PWM0_0_LOAD_R = PWM_LOAD_VALUE;
    PWM0_1_LOAD_R = PWM_LOAD_VALUE;
    PWM0_2_LOAD_R = PWM_LOAD_VALUE;
    PWM0_3_LOAD_R = PWM_LOAD_VALUE;
    
    // Initial Duty (Center)
    center = Map_Angle_To_Duty(90); // FIXED: Assignment happens after declarations
    PWM0_0_CMPA_R = center;
    PWM0_1_CMPA_R = center;
    PWM0_2_CMPA_R = center;
    PWM0_3_CMPA_R = Map_Angle_To_Duty(0); // Claw Open
    
    // Enable Generators
    PWM0_0_CTL_R |= 1; PWM0_1_CTL_R |= 1; PWM0_2_CTL_R |= 1; PWM0_3_CTL_R |= 1;
    
    // Enable Outputs (Bit 0, 2, 4, 6) -> 0x55
    PWM0_ENABLE_R |= 0x55;
}

// --- Interrupt Handler ---
void UART1_Handler(void) {
    uint8_t data;
    
    // 1. Clear Interrupt Flag
    UART1_ICR_R = 0x0010; 
    
    // 2. Read Data
    data = (uint8_t)(UART1_DR_R & 0xFF);
    
    // 3. State Machine
    switch(rx_state) {
        case 0: // Wait for Header
            if(data == HEADER_BYTE) {
                rx_state = 1;
                rx_index = 0;
            }
            break;
            
        case 1: // Data Bytes
            rx_buffer[rx_index++] = data;
            if(rx_index >= 4) {
                rx_state = 2; // Data full, check footer
            }
            break;
            
        case 2: // Wait for Footer
            if(data == FOOTER_BYTE) {
                // VALID PACKET RECEIVED!
                angle_base     = rx_buffer[0];
                angle_shoulder = rx_buffer[1];
                angle_elbow    = rx_buffer[2];
                angle_claw     = rx_buffer[3];
                
                // Move the robot
                Move_Servos();
            }
            // Reset regardless of valid footer or not
            rx_state = 0;
            break;
            
        default:
            rx_state = 0;
            break;
    }
}

// --- Main ---
int main(void) {
    // 1. Initialize
    PWM_Init();
    UART1_Init();
    __enable_irq(); 
    
    while(1) {
        // IDLE LOOP
        // Robot moves only when UART Interrupt fires.
    }
}
