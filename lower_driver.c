void HAL_LCD_PortInit(void)
{
    // SPI Pins (P1.4 Clock, P1.6 SIMO) - These look correct
    P1SEL1 &= ~BIT4;
    P1SEL0 |= BIT4;
    P1SEL1 &= ~BIT6;
    P1SEL0 |= BIT6;

    // Reset Pin - The driver (lcd_driver.c) uses P9.4
    // We MUST initialize P9.4 here as an output
    P9DIR |= BIT4;
    P9OUT |= BIT4;

    // Data/Command Pin (P2.3)
    P2DIR |= BIT3;

    // Chip Select (P2.4)
    P2DIR |= BIT4;
    P2OUT &= ~BIT4; // Enabled (Low)

    // Backlight (P2.6) - Required for the MKII display to be visible
    P2DIR |= BIT6;
    P2OUT |= BIT6;

    // Unlock GPIO
    PM5CTL0 &= ~LOCKLPM5;

    return;
}

void HAL_LCD_SpiInit(void)
{
    // Put eUSCI in reset
    UCB0CTLW0 = UCSWRST;

    // CORRECTED SPI SETTINGS:
    // 1. Remove UC7BIT (LCD needs 8-bit, and 8-bit is the default 0)
    // 2. UCCKPH = Capture on first edge
    // 3. UCMSB = MSB first
    // 4. UCMST = Master mode
    // 5. UCSYNC = Synchronous
    // 6. UCSSEL_2 = SMCLK
    UCB0CTLW0 |= UCCKPH | UCMSB | UCMST | UCMODE_0 | UCSYNC | UCSSEL_2;

    // Clock divider: 16MHz / 2 = 8MHz (Stay under 10MHz limit)
    UCB0BRW = 2;

    // Exit reset
    UCB0CTLW0 &= ~UCSWRST;

    // Default pin states
    P2OUT &= ~BIT4; // CS Low
    P2OUT &= ~BIT3; // DC Low

    return;
}
