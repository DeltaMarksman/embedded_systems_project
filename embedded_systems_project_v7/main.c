#include "msp430fr6989.h"
#include <stdint.h>

// VT100 terminal escape codes
#define VT100_CURSOR_OFF  "\033[?25l"
#define VT100_CLEAR_LINE  "\033[2K\r"
#define COLOR_RED         "\033[31m"
#define COLOR_GREEN       "\033[32m"
#define COLOR_YELLOW      "\033[33m"
#define COLOR_EXIT        "\033[0m"

// Joystick status bitmasks
#define IDL  0x01
#define LOW  0x02
#define MED  0x04
#define HIG  0x08
#define POS  0x10
#define NEG  0x20

// Deflection thresholds
#define LOW_THRESHOLD  3
#define MED_THRESHOLD  40
#define HIG_THRESHOLD  80

// Globals
volatile uint16_t result_x, result_y;
volatile uint16_t delta_x,  delta_y;
volatile uint8_t  status_x, status_y;
volatile uint8_t  update_flag = 0; // Flag to trigger screen update

// Prototypes
void clock_system_initialize_16MHz(void);
void adc_initialize(void);
void uart_initialize(void);
void uart_write_char(char c);
void uart_write_string(const char *s);
void uart_write_uint16(uint16_t val);
void update_screen(void);

void main(void)
{
    WDTCTL  = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    clock_system_initialize_16MHz();
    uart_initialize();
    adc_initialize();

    ADC12IER0 |= ADC12IE0 | ADC12IE1;

    // Timer for ADC Trigger (~10Hz)
    TA0CCR0   = 3276;
    TA0CCTL0 |= CCIE;
    TA0CTL    = TASSEL_1 | MC_1 | TACLR;

    // Timer for Screen Update Flag (~5Hz)
    TA1CCR0   = 6553;
    TA1CCTL0 |= CCIE;
    TA1CTL    = TASSEL_1 | MC_1 | TACLR;

    uart_write_string(VT100_CURSOR_OFF);
    uart_write_string("\033[2J"); // Clear entire screen initially

    __enable_interrupt();

    while (1) {
        if (update_flag) {
            update_screen();
            update_flag = 0; // Reset flag after printing
        }
    }
}

void update_screen(void)
{
    uint16_t temp;

    // Update X row
    uart_write_string("\033[1;1H");
    uart_write_string(VT100_CLEAR_LINE);
    uart_write_string("x ");
    uart_write_string((status_x & POS) ? "+ " : "- ");

    if      (status_x & HIG) uart_write_string(COLOR_RED);
    else if (status_x & MED) uart_write_string(COLOR_YELLOW);
    else                     uart_write_string(COLOR_GREEN);

    uart_write_uint16(result_x);
    uart_write_string(COLOR_EXIT);

    // Update Y row
    uart_write_string("\033[3;1H");
    uart_write_string(VT100_CLEAR_LINE);
    uart_write_string("y ");
    uart_write_string((status_y & POS) ? "+ " : "- ");

    if      (status_y & HIG) uart_write_string(COLOR_RED);
    else if (status_y & MED) uart_write_string(COLOR_YELLOW);
    else                     uart_write_string(COLOR_GREEN);

    uart_write_uint16(result_y);
    uart_write_string(COLOR_EXIT);
}

#pragma vector = TIMER1_A0_VECTOR
__interrupt void TA1_A0_ISR(void)
{
    update_flag = 1; // Signal main loop to print
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void TA0_A0_ISR(void)
{
    ADC12CTL0 |= ADC12SC; // Trigger ADC
}

#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
{
    if (ADC12IFGR0 & ADC12IFG0)
    {
        result_x = (uint16_t)(((uint32_t)ADC12MEM0 * 25) >> 9);
        delta_x  = (result_x >= 100) ? result_x - 100 : 100 - result_x;
        status_x = 0;
        if      (result_x > 100 + LOW_THRESHOLD) status_x |= POS;
        else if (result_x < 100 - LOW_THRESHOLD) status_x |= NEG;
        if      (delta_x > HIG_THRESHOLD) status_x |= HIG;
        else if (delta_x > MED_THRESHOLD) status_x |= MED;
        else                              status_x |= LOW;
    }

    if (ADC12IFGR0 & ADC12IFG1)
    {
        result_y = (uint16_t)(((uint32_t)ADC12MEM1 * 25) >> 9);
        delta_y  = (result_y >= 100) ? result_y - 100 : 100 - result_y;
        status_y = 0;
        if      (result_y > 100 + LOW_THRESHOLD) status_y |= POS;
        else if (result_y < 100 - LOW_THRESHOLD) status_y |= NEG;
        if      (delta_y > HIG_THRESHOLD) status_y |= HIG;
        else if (delta_y > MED_THRESHOLD) status_y |= MED;
        else                              status_y |= LOW;
    }
}

void clock_system_initialize_16MHz(void)
{
    FRCTL0  = FRCTLPW | NWAITS_1;
    CSCTL0  = CSKEY;
    CSCTL1  = DCOFSEL_4 | DCORSEL; // Set DCO to 16MHz
    CSCTL2  = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3  = DIVA__1 | DIVS__1 | DIVM__1; // No dividers = SMCLK @ 16MHz
    CSCTL0_H = 0;
}

void adc_initialize(void)
{
    // Configure Pins: P9.2 (A10) and P8.2 (A6)
    P9SEL1 |= BIT2;  P9SEL0 |= BIT2;
    P8SEL1 |= BIT2;  P8SEL0 |= BIT2;

    ADC12CTL0 &= ~ADC12ENC;
    // SHT_7 = 192 cycles for sampling (more stable), MSC = Multiple Sample Conversion
    ADC12CTL0  = ADC12ON | ADC12SHT0_7 | ADC12MSC;
    // CONSEQ_1 = Sequence of channels A10 then A6
    ADC12CTL1  = ADC12SHP | ADC12CONSEQ_1;
    ADC12CTL2  = ADC12RES_2; // 12-bit resolution

    // Map Memory 0 to A10 (X) and Memory 1 to A6 (Y)
    ADC12MCTL0 = ADC12INCH_10;
    ADC12MCTL1 = ADC12INCH_6 | ADC12EOS; // EOS = End of Sequence

    ADC12CTL0 |= ADC12ENC;
}

void uart_initialize(void)
{
    P3SEL0 |=  BIT4 | BIT5; // USCI_A1 UART pins
    P3SEL1 &= ~(BIT4 | BIT5);

    UCA1CTLW0 = UCSWRST;
    UCA1CTLW0 |= UCSSEL__SMCLK;

    // 16,000,000 / 9600 = 1666.666
    // Using Oversampling (UCOS16 = 1):
    // 1666.666 / 16 = 104.166 -> UCBR = 104
    // Fractional portion 0.166 -> UCBRF = 2 (from table)
    UCA1BRW    = 104;
    UCA1MCTLW  = 0xD600 | UCOS16 | UCBRF_2; // 0xD6 is UCBRS = 0xD6

    UCA1CTLW0 &= ~UCSWRST;
}

void uart_write_char(char c)
{
    while (!(UCA1IFG & UCTXIFG));
    UCA1TXBUF = c;
}

void uart_write_string(const char *s)
{
    while (*s) uart_write_char(*s++);
}

void uart_write_uint16(uint16_t val)
{
    char buf[6];
    uint8_t i = 0;
    if (val == 0) { uart_write_char('0'); return; }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i--) uart_write_char(buf[i]);
}
