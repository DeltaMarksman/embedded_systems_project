/* --COPYRIGHT--,BSD
 * Copyright (c) 2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//*****************************************************************************
//         GUI Composer Simple JSON Demo using MSP430
//
// Texas Instruments, Inc.
// ******************************************************************************

#include <msp430.h>

#define FLAGS UCA1IFG // Contains the transmit & receive flags
#define RXFLAG UCRXIFG // Receive flag
#define TXFLAG UCTXIFG // Transmit flag
#define TXBUFFER UCA1TXBUF // Transmit buffer
#define RXBUFFER UCA1RXBUF // Receive buffer
#define true 1
#define false 0

void uart_write_char(unsigned char ch){
    // Wait for any ongoing transmission to complete
    while ( (FLAGS & TXFLAG)==0 ) {}

    // Copy the byte to the transmit buffer
    TXBUFFER = ch; // Tx flag goes to 0 and Tx begins!

    return;
}

void uart_newline() {
    uart_write_char('\n');
    uart_write_char('\r');
}

void uart_write_uint16(unsigned int n) {
    unsigned int divisor = 10000;
    unsigned char started = 0;

    if (n == 0) {
        uart_write_char('0');
        return;
    }

    while (divisor > 0) {
        unsigned int digit = n / divisor;
        if (digit > 0 || started) {
            uart_write_char('0' + digit);
            started = 1;
        }
        n %= divisor;
        divisor /= 10;
    }
}

void uart_write_string(char *str) {
    while (*str != '\0') {
        uart_write_char(*str);
        str++;
    }

    //uart_newline();
}

void uart_write_charln(unsigned char ch) {
    uart_write_char(ch);
    uart_newline();
}


// The function returns the byte; if none received, returns null character
unsigned char uart_read_char(void){
    unsigned char temp;
    // Return null character (ASCII=0) if no byte was received
    if( (FLAGS & RXFLAG) == 0)
        return 0;

    // Otherwise, copy the received byte (this clears the flag) and return it
    temp = RXBUFFER;
    return temp;
}


char* uart_read_string()
{
    unsigned int i = 0;
    static char buffer[32];

    while (i < 32) {
        unsigned char c = uart_read_char();

        // no byte received yet
        if (c == 0) {
            continue;
        }

        // end of line
        if (c == '\n' || c == '\r') {
            break;
        }

        buffer[i++] = c;
    }

    // null-terminate
    buffer[i] = '\0';           // null-terminate

    return buffer;
}

// Configure UART to the popular configuration
// 9600 baud, 8-bit data, LSB first, no parity bits, 1 stop bit
// no flow control, oversampling reception
// Clock: SMCLK @ 1 MHz (1,000,000 Hz)
void Initialize_UART(void){
    // Configure pins to UART functionality
    P3SEL1 &= ~(BIT4|BIT5);
    P3SEL0 |= (BIT4|BIT5);

    // Main configuration register
    UCA1CTLW0 = UCSWRST; // Engage reset; change all the fields to zero

    // Most fields in this register, when set to zero, correspond to the
    // popular configuration
    UCA1CTLW0 |= UCSSEL_2; // Set clock to SMCLK

    // Configure the clock dividers and modulators (and enable oversampling)
    UCA1BRW = 6; // divider

    // Modulators: UCBRF = 8 = 1000 --> UCBRF3 (bit #3)
    // UCBRS = 0x20 = 0010 0000 = UCBRS5 (bit #5)
    UCA1MCTLW = UCBRF3 | UCBRS5 | UCOS16;

    // Exit the reset state
    UCA1CTLW0 &= ~UCSWRST;
}

//**********************************
// Configures ACLK to 32 KHz crystal
void config_ACLK_to_32KHz_crystal(void) {
    // By default, ACLK runs on LFMODCLK at 5MHz/128 = 39 KHz

    // Reroute pins to LFXIN/LFXOUT functionality
    PJSEL1 &= ~BIT4;
    PJSEL0 |= BIT4;

    // Wait until the oscillator fault flags remain cleared
    CSCTL0 = CSKEY; // Unlock CS registers

    do {
        CSCTL5 &= ~LFXTOFFG; // Local fault flag
        SFRIFG1 &= ~OFIFG; // Global fault flag
    } while((CSCTL5 & LFXTOFFG) != 0);


    CSCTL0_H = 0; // Lock CS registers
    return;
}



unsigned long prng_state;
unsigned int random_number(int max) {
    //https://en.wikipedia.org/wiki/Linear_congruential_generator
    prng_state = (1664525UL * prng_state + 1013904223UL);
    return abs(prng_state) % max;
}


void setup_prng() {
    // Setup clock
    // Use ACLK, divide by 1, continuous mode, clear TAR
    TA0CTL = TASSEL_1 | ID_0 | MC_2 | TACLR;
    int i = 0;
    for (i = 0; i < 20000; i++) {}

    // Since ACLK is not configed, and using for loop, value of TA0R is undeterministic
    // We can use this for the random seed.
    prng_state = TA0R;
}

// Will determine the sequence of the notes to play from 1-7

void generate_sequence(unsigned int sequence[]) {
    int i;
    for (i = 0; i < 3; i++) {
        sequence[i] = random_number(7);

        // Ensure no duplicates
        if (i > 0 && sequence[i] == sequence[i-1]) {
            // repeat loop without increment
            i--;
            continue;
        }
    }

    static const int print = 1;
    if (!print) return;

    for (i = 0; i < 3; i++) {
        uart_write_uint16(sequence[i]);
        uart_newline();
    }
}

/*
 * Clock cycles is found by dividing 32khz by frequency *2 (square wave)
 * */
int note_to_clk_cycles_32KHZ[7] = {
    37, // A4
    33, // B4
    31, // C5
    28, // D5
    25, // E5
    23, // F5
    21  // G5
};

void config_clk() {
    // Configure Channel 0 for up mode with interrupts
    TA0CCR0 = note_to_clk_cycles_32KHZ[0]; //@ 32KHz, 1 second = 2^16
    TA0CCTL0 |= CCIE;
    TA0CCTL0 &= ~CCIFG;

    // Configure Timer_A
    // Use ACLK, divide by 1, up mode, TAR cleared
    TA0CTL = TASSEL_1 | ID_0 | MC_0 | TACLR ;
}

void config_piezo() {
    P2DIR  |= BIT7;
}

void play_note(int note) {
    // Turn timer on
    TA0CTL |= MC_1;
    TA0CCR0 = note_to_clk_cycles_32KHZ[note];
}

void stop_note() {
    // Turn timer off
    TA0CTL = (TA0CTL & ~MC_3);
}

void play_round() {
    // Generate sequence
    unsigned int sequence[3] = {0};
    generate_sequence(sequence);

    // play sequence
    int i;
    for (i = 0; i < 3; i++) {
        play_note(sequence[i]);
        _delay_cycles(500000);
        stop_note();
        _delay_cycles(500000);
    }

}

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    PM5CTL0 &= ~LOCKLPM5; // Disable GPIO power-on default high-impedance mode

    // UART
    Initialize_UART();

    // Generate sequence of 3 notes. Use as many as needed=
    setup_prng();

    // Init clk and piezo
    config_ACLK_to_32KHz_crystal();
    config_clk();
    config_piezo(); // Default tone of 440
    _enable_interrupts();

    play_round();
}

//******* Writing the ISR *******
#pragma vector = TIMER0_A0_VECTOR // Link the ISR to the vector
__interrupt void T0A0_ISR() {
    // Interrupt response goes here
    P2OUT ^= BIT7; // toggle piezo which gives square wave
}
