#include "msp430fr6989.h"
#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

//joystick bit masks
#define POS  0x10
#define NEG  0x20
#define LOW_THRESHOLD  3

//buttons
#define BUT1 BIT1 //S1 when pressed after user as indicated number of rounds will initiate the game rounds
#define BUT2 BIT2 //S2 when pressed allows user to start over to start menu

// Globals
volatile uint16_t result_x, result_y;
volatile uint8_t  status_x, status_y;
volatile uint8_t  update_flag = 0;
volatile uint8_t joystick_moved = 0;
volatile uint8_t rounds =0;
volatile uint8_t button_state = 0;
volatile uint8_t button2_state = 0;
volatile uint8_t game_start = 0;
volatile uint8_t score = 0;
volatile int user_answer[2] = {0};
Graphics_Context g_sContext;
int note_to_clk_cycles_16MHZ[7] = {18181, 16200, 15288, 13619, 12133, 11448, 10200};


//clock for what??
void Initialize_Clock_System() {
  // DCO frequency = 16 MHz
  // MCLK = fDCO/1 = 16 MHz
  // SMCLK = fDCO/1 = 16 MHz

  // Activate memory wait state
  FRCTL0 = FRCTLPW | NWAITS_1;    // Wait state=1
  CSCTL0 = CSKEY;
  // Set DCOFSEL to 4 (3-bit field)
  CSCTL1 &= ~DCOFSEL_7;
  CSCTL1 |= DCOFSEL_4;
  // Set DCORSEL to 1 (1-bit field)
  CSCTL1 |= DCORSEL;
  // Change the dividers to 0 (div by 1)
  CSCTL3 &= ~(DIVS2|DIVS1|DIVS0);    // DIVS=0 (3-bit)
  CSCTL3 &= ~(DIVM2|DIVM1|DIVM0);    // DIVM=0 (3-bit)
  CSCTL0_H = 0;

  return;
}
//graphics
void set_graphics() {
    Crystalfontz128x128_Init();
    Crystalfontz128x128_SetOrientation(0);
    Graphics_initContext(&g_sContext, &g_sCrystalfontz128x128);
    Graphics_setBackgroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);
    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
    Graphics_clearDisplay(&g_sContext);
}
//adc
void adc_initialize(void) {
    P9SEL1 |= BIT2; P9SEL0 |= BIT2;
    P8SEL1 |= BIT7; P8SEL0 |= BIT7;
    ADC12CTL0 &= ~ADC12ENC;
    ADC12CTL0  = ADC12ON | ADC12SHT0_7 | ADC12MSC;
    ADC12CTL1  = ADC12SHP | ADC12CONSEQ_1;
    ADC12CTL2  = ADC12RES_2;
    ADC12MCTL0 = ADC12INCH_10;
    ADC12MCTL1 = ADC12INCH_4 | ADC12EOS;
    ADC12CTL0 |= ADC12ENC;
    ADC12IER0 |= ADC12IE0 | ADC12IE1;

    TA0CCR0   = 3276;
    TA0CCTL0 |= CCIE;
    TA0CTL    = TASSEL_1 | MC_1 | TACLR;
}
void config_clk() {
    // Configure Channel 0 for up mode with interrupts
    TB0CCR0 = note_to_clk_cycles_16MHZ[0]; //@ 32KHz, 1 second = 2^16
    TB0CCTL0 |= CCIE;
    TB0CCTL0 &= ~CCIFG;

    // Configure Timer_A
    // Use SMCLK, divide by 1, up mode, TAR cleared
    TB0CTL = TBSSEL_2 | ID_0 | MC_0 | TBCLR ;
}

//music stuff --------------------------------------------------------------------------------------------------------music stuff
unsigned long prng_state;
void config_piezo() {
    P2DIR  |= BIT7;
}
unsigned int random_number(int max) {
    //https://en.wikipedia.org/wiki/Linear_congruential_generator
    prng_state = (1664525UL * prng_state + 1013904223UL);
    return abs(prng_state) % max;
}
void setup_prng() {
    prng_state = 0;
}
void play_note(int note) {
    TB0CTL |= MC_1;
    TB0CCR0 = note_to_clk_cycles_16MHZ[note];
}
void stop_note() {
    TB0CTL = (TB0CTL & ~MC_3);
}
void generate_sequence(unsigned int sequence[] ) {
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
}

void sequence_answer(unsigned int sequence[]){
    unsigned int answer[]={0};
    if(sequence[1]>sequence[0]){
        answer[0] = 1;
    }
    if(sequence[1]<sequence[0]){
            answer[0] = 0;
        }
    if(sequence[2]>sequence[1]){
            answer[1] = 1;
        }
    if(sequence[2]<sequence[1]){
            answer[1] = 0;
        }
}

//game functions------------------------------------------------------------------------------------------------------game functions
void start_menu(){
    char buffer[10]={0};
    // Convert number to string
    sprintf(buffer, "%d", rounds);
    // Draw the string on the screen
    Graphics_drawStringCentered(&g_sContext,(int8_t *)buffer, AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);
}

void play_game(){
    int r = 0;
    for (r=0; r<rounds; r++){
        Graphics_clearDisplay(&g_sContext);
        unsigned int sequence[3]={0};
        generate_sequence(sequence);
        int answer[2] = {sequence[1] > sequence[0], sequence[2] > sequence[1]};


        int i;
        for (i = 0; i < 3; i++) {
            //display note
            char str[10]={0};
            sprintf(str, "%d", i);
            Graphics_drawStringCentered(&g_sContext, "note: ", AUTO_STRING_LENGTH, 64, 30, OPAQUE_TEXT);
            Graphics_drawStringCentered(&g_sContext,(int8_t *)str, AUTO_STRING_LENGTH, 64, 50, OPAQUE_TEXT);

            play_note(sequence[i]);
            _delay_cycles(5000000);
            stop_note();
            _delay_cycles(5000000);

            //print answer
            char buffer[10]={0};
            sprintf(buffer, "%d", answer[i]);
            Graphics_drawStringCentered(&g_sContext,(int8_t *)buffer, AUTO_STRING_LENGTH, 64, 70, OPAQUE_TEXT);
        }
    Graphics_clearDisplay(&g_sContext);
    rounds = 0;
    }
}

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    //itialize stuff
    setup_prng(); //starting three notes
    Initialize_Clock_System(); //clock for who knows what
    config_piezo(); // Default tone of 440
    config_clk(); //timerB
    adc_initialize();
    set_graphics();

    //configure buttons
    P1DIR &= ~(BUT1|BUT2); // 0: input
    P1REN |= (BUT1|BUT2); // 1: enable built-in resistors
    P1OUT |= (BUT1|BUT2); // 1: built-in resistor is pulled up to Vcc
    P1IES |= (BUT1|BUT2); // 1: interrupt on falling edge (0 for rising edge)
    P1IFG &= ~(BUT1|BUT2); // 0: clear the interrupt flags
    P1IE |= (BUT1|BUT2); // 1: enable the interrupts

    _enable_interrupts();
    while(1){
        Graphics_drawStringCentered(&g_sContext, "Choose Rounds", AUTO_STRING_LENGTH, 64, 30, OPAQUE_TEXT);
        if(!button_state || button2_state){
            Graphics_drawStringCentered(&g_sContext, "Choose Rounds", AUTO_STRING_LENGTH, 64, 30, OPAQUE_TEXT);
            Graphics_drawStringCentered(&g_sContext, "Press S1 to Start", AUTO_STRING_LENGTH, 64, 100, OPAQUE_TEXT);
            button2_state =0;
            _enable_interrupts();
            start_menu();
        }

        if(button_state && !button2_state){
            button_state = 0;
            play_game();
        }

    }
}

#pragma vector = TIMER0_B0_VECTOR
__interrupt void T0_B0_ISR() {
    P2OUT ^= BIT7;
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void TA0_A0_ISR() {
  ADC12CTL0 |= ADC12SC;
  return;
}

//gets joystick info
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
{
    if (ADC12IFGR0 & ADC12IFG0)
    {
        result_x =(uint16_t)(((uint32_t)ADC12MEM0 * 200) >> 12);
        if (result_x > 160) {
            rounds++;
            __delay_cycles(1000000); // Simple debounce
        }
        if (result_x < 40 && rounds > 0) {
            rounds--;
            __delay_cycles(1000000); // Simple debounce
        }
    }

    if (ADC12IFGR0 & ADC12IFG1)
    {
        result_y = ADC12MEM1*200/4096;
        status_y = 0;
        if(result_y > 100 + LOW_THRESHOLD){
            status_y |= POS;
        }
        else if(result_y < 100 - LOW_THRESHOLD){
            status_y |= NEG;
        }
    }

}

//to commence game from start menu
#pragma vector = PORT1_VECTOR
__interrupt void Port1_ISR(void){
    P1IE &= ~BUT1;
    _delay_cycles(40000);

    if(P1IFG & BUT1){
        button_state = 1;
        P1IFG &= ~BUT1;
    }

    if(P1IFG & BUT2){
        button2_state = 1;
        button_state = 0;
        game_start=0;
        P1IFG &= ~BUT2;
    }
}
