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

#include "msp430fr6989.h"
#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


#define FLAGS UCA1IFG // Contains the transmit & receive flags
#define RXFLAG UCRXIFG // Receive flag
#define TXFLAG UCTXIFG // Transmit flag
#define TXBUFFER UCA1TXBUF // Transmit buffer
#define RXBUFFER UCA1RXBUF // Receive buffer
#define true 1
#define false 0


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

//buttons
#define BUT1 BIT1 //S1 when pressed after user as indicated number of rounds will initiate the game rounds
#define BUT2 BIT2 //S2 when pressed allows user to start over to start menu

// Globals
int result_x, result_y;
int delta_x,  delta_y;
int status_x, status_y;
int update_flag         = 0; // Flag to trigger screen update
int joystick_is_up      = 0;
int joystick_is_down    = 0;
int rounds              = 0;
Graphics_Context g_sContext;


typedef enum {
    CHOOSING_ROUNDS,
    PLAYING_NOTES,
    WAITING_ON_ANSWER,
    DISPLAY_FEEDBACK,
    DISPLAY_FINAL_RESULTS,
} game_state_t;
game_state_t game_state = CHOOSING_ROUNDS;

// Prototypes
void clock_system_initialize_16MHz(void);
void adc_initialize(void);
void update_screen(void);

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
// Clock: 16 MHz (16,000,000 Hz)
void Initialize_UART(void){
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

void clock_system_initialize_16MHz(void)
{
    FRCTL0  = FRCTLPW | NWAITS_1;
    CSCTL0  = CSKEY;
    CSCTL1  = DCOFSEL_4 | DCORSEL; // Set DCO to 16MHz
    CSCTL2  = SELA__LFXTCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3  = DIVA__1 | DIVS__1 | DIVM__1; // No dividers = SMCLK @ 16MHz
    CSCTL0_H = 0;
}

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

void adc_initialize(void)
{
    // Configure Pins: P9.2 (A10) and P8.7 (A4)
    P9SEL1 |= BIT2;  P9SEL0 |= BIT2;
    P8SEL1 |= BIT7;  P8SEL0 |= BIT7;

    ADC12CTL0 &= ~ADC12ENC;
    // SHT_7 = 192 cycles for sampling (more stable), MSC = Multiple Sample Conversion
    ADC12CTL0  = ADC12ON | ADC12SHT0_7 | ADC12MSC;
    // CONSEQ_1 = Sequence of channels A10 then A4
    ADC12CTL1  = ADC12SHP | ADC12CONSEQ_1;
    ADC12CTL2  = ADC12RES_2; // 12-bit resolution

    // Map Memory 0 to A10 (X) and Memory 1 to A4 (Y)
    ADC12MCTL0 = ADC12INCH_10;
    ADC12MCTL1 = ADC12INCH_4 | ADC12EOS; // EOS = End of Sequence

    ADC12CTL0 |= ADC12ENC;

    ADC12IER0 |= ADC12IE0 | ADC12IE1;

    // Timer for ADC Trigger (~10Hz)
    TA0CCR0   = 3276;
    TA0CCTL0 |= CCIE;
    TA0CTL    = TASSEL_1 | MC_1 | TACLR;
}

void set_graphics() {
    Crystalfontz128x128_Init();
    Crystalfontz128x128_SetOrientation(0);
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
    Graphics_clearDisplay(&g_sContext);
}

unsigned long prng_state;
unsigned int random_number(int max) {
    //https://en.wikipedia.org/wiki/Linear_congruential_generator
    prng_state = (1664525UL * prng_state + 1013904223UL);
    return abs(prng_state) % max;
}


void setup_prng() {
    prng_state = 0;
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

    static const int print = 0;
    if (!print) return;

    for (i = 0; i < 3; i++) {
        uart_write_uint16(sequence[i]);
        uart_newline();
    }
}

/*
 * Clock cycles is found by dividing 32khz by frequency *2 (square wave)
 * */
int note_to_clk_cycles_16MHZ[7] = {
    18181, // A4
    16200, // B4
    15288, // C5
    13619, // D5
    12133, // E5
    11448, // F5
    10200  // G5
};

void config_clk() {
    // Configure Channel 0 for up mode with interrupts
    TB0CCR0 = note_to_clk_cycles_16MHZ[0]; //@ 32KHz, 1 second = 2^16
    TB0CCTL0 |= CCIE;
    TB0CCTL0 &= ~CCIFG;

    // Configure Timer_A
    // Use SMCLK, divide by 1, up mode, TAR cleared
    TB0CTL = TBSSEL_2 | ID_0 | MC_0 | TBCLR ;
}

void config_piezo() {
    P2DIR  |= BIT7;
}

void play_note(int note) {
    // Turn timer on
    TB0CTL |= MC_1;
    TB0CCR0 = note_to_clk_cycles_16MHZ[note];
}

void play_frequency(int frequency) {
    // Turn timer on
    TB0CTL |= MC_1;
    TB0CCR0 = frequency;
}

void stop_note() {
    // Turn timer off
    TB0CTL = (TB0CTL & ~MC_3);
}

void play_round(int answer[]) {
    // Generate sequence
    unsigned int sequence[3] = {0};
    generate_sequence(sequence);

    // Generate answers
    int round_answer[2] = {sequence[1] > sequence[0], sequence[2] > sequence[1]};



    // play sequence
    int i;
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawStringCentered(&g_sContext,"Playing...", AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);
    for (i = 0; i < 3; i++) {
        // Printing
        uart_write_string("Playing note: ");
        uart_write_uint16(sequence[i]);
        uart_newline();


        play_note(sequence[i]);
        _delay_cycles(5000000);
        stop_note();
        _delay_cycles(5000000);
    }

    // Return the answers
    uart_write_string("Correct Answer is: ");
    uart_write_uint16(round_answer[0]);
    uart_write_string(", ");
    uart_write_uint16(round_answer[1]);
    uart_newline();

    answer[0] = round_answer[0];
    answer[1] = round_answer[1];
}

int check_answer(int answer[], int answer_input[]) {
    return (answer[0]==answer_input[0]) && (answer[1]==answer_input[1]);
}

void display_correct() {
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawStringCentered(&g_sContext, "CORRECT!", AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);
    uart_write_string("CORRECT!");
    _delay_cycles(50000000);
}

void display_incorrect() {
    Graphics_clearDisplay(&g_sContext);
    Graphics_drawStringCentered(&g_sContext,"WRONG!", AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);
    uart_write_string("WRONG!");
    _delay_cycles(50000000);
}

void display_final_results(int num_correct) {
    Graphics_clearDisplay(&g_sContext);
    char buff[10];
    sprintf(buff, "%d/%d", num_correct, rounds);
    Graphics_drawStringCentered(&g_sContext,(int8_t*)buff, AUTO_STRING_LENGTH, 60, 50, OPAQUE_TEXT);
}

void play_kirk() {

    // A
    play_note(0);
    _delay_cycles(10000000);
    stop_note();
    _delay_cycles(100000);

    // A
    play_note(0);
    _delay_cycles(10000000);
    stop_note();
    _delay_cycles(100000);

    // D
    play_note(3);
    _delay_cycles(15000000);
    stop_note();
    _delay_cycles(100000);

    // E
    play_note(4);
    _delay_cycles(5000000);
    stop_note();
    _delay_cycles(100000);

    // F
    play_note(5);
    _delay_cycles(30000000);
    stop_note();
    _delay_cycles(100000);

    // F
    play_note(5);
    _delay_cycles(5000000);
    stop_note();
    _delay_cycles(100000);

    // F
    play_note(5);
    _delay_cycles(30000000);
    stop_note();
    _delay_cycles(100000);

    // E
    play_note(4);
    _delay_cycles(5000000);
    stop_note();
    _delay_cycles(100000);

    // D
    play_note(3);
    _delay_cycles(10000000);
    stop_note();
    _delay_cycles(100000);

    // Bb
    play_frequency(17161);
    _delay_cycles(40000000);
    stop_note();
    _delay_cycles(100000);
}


void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    PM5CTL0 &= ~LOCKLPM5; // Disable GPIO power-on default high-impedance mode

    // Generate sequence of 3 notes. Use as many as needed
    setup_prng();

    // Init clk and piezo
    clock_system_initialize_16MHz();


    // Uart and piezo
    Initialize_UART();
    config_clk();
    config_piezo(); // Default tone of 440
    set_graphics();

    // ADC
    adc_initialize();

    //configure buttons
    P1DIR &= ~(BUT1|BUT2); // 0: input
    P1REN |= (BUT1|BUT2); // 1: enable built-in resistors
    P1OUT |= (BUT1|BUT2); // 1: built-in resistor is pulled up to Vcc
    P1IES |= (BUT1|BUT2); // 1: interrupt on falling edge (0 for rising edge)
    P1IFG &= ~(BUT1|BUT2); // 0: clear the interrupt flags
    P1IE |= (BUT1|BUT2); // 1: enable the interrupts

    _enable_interrupts();

    uart_write_string("Hello embedded world!");

    play_kirk();

    // GAME START WITH CHOOSING ROUNDS
    int num_correct = 0;
    while (game_state == CHOOSING_ROUNDS) {
        Graphics_drawStringCentered(&g_sContext,"Choose # Rounds" , AUTO_STRING_LENGTH, 64, 30, OPAQUE_TEXT);
        if (joystick_is_up){
            rounds++;
            char choose[10];
            sprintf(choose, "  %d  ", rounds);
            Graphics_drawStringCentered(&g_sContext,(int8_t*)choose, AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);

            uart_write_string("Joystick is up");
            while (joystick_is_up) {}

        }
        if (joystick_is_down){
            if (rounds == 0) continue;
            rounds--;
            char choose[10];
            sprintf(choose, "  %d  ", rounds);
            Graphics_drawStringCentered(&g_sContext,(int8_t*)choose, AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);

            uart_write_string("Joystick is down");
            while (joystick_is_down) {}
        }
    }
    Graphics_clearDisplay(&g_sContext);

    // PLAY NOTES FOR ROUND
    int round_index;
    for (round_index = 0; round_index < rounds; round_index++) {
        // Play notes
        game_state = PLAYING_NOTES;
        int answer[2] = {0};
        play_round(answer);
        Graphics_clearDisplay(&g_sContext);

        // Waiting on user input to drive answer_input
        game_state = WAITING_ON_ANSWER;
        Graphics_clearDisplay(&g_sContext);
        Graphics_drawStringCentered(&g_sContext,"What is the sequence?", AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);
        int answer_input[2];
        int answer_input_index = 0;

        // Wait for input
        while (game_state == WAITING_ON_ANSWER) {
            if (answer_input_index > 1) {
                game_state = DISPLAY_FEEDBACK;
                break;
            }

            if (joystick_is_up){
                answer_input[answer_input_index] = 1;
                while (joystick_is_up) {}
                answer_input_index++;
                continue;
            }

            if (joystick_is_down){
                answer_input[answer_input_index] = 0;
                while (joystick_is_down) {}
                answer_input_index++;
                continue;
            }
        }

        uart_write_uint16(answer_input[0]);
        uart_write_uint16(answer_input[1]);

        // Display feedback
        int round_result = check_answer(answer, answer_input);
        if (round_result) {
            display_correct();
            num_correct++;
        } else {
            display_incorrect();
        }
    }

    display_final_results(num_correct);

}

//******* PIEZO SQUARE WAVE *******
#pragma vector = TIMER0_B0_VECTOR // Link the ISR to the vector
__interrupt void T0B0_ISR() {
    // Interrupt response goes here
    P2OUT ^= BIT7; // toggle piezo which gives square wave
}

//******* UPDATE ADC *******
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

        joystick_is_up = (status_y & HIG) && (status_y & POS);
        joystick_is_down = (status_y & HIG) && !(status_y & POS);
    }

}

#pragma vector = PORT1_VECTOR
__interrupt void Port1_ISR(void){
    _delay_cycles(40000);

    if(P1IFG & BUT1){
        P1IFG &= ~BUT1;
        if (game_state == CHOOSING_ROUNDS){
            game_state = PLAYING_NOTES;
            Graphics_clearDisplay(&g_sContext);
        }
    }
}
