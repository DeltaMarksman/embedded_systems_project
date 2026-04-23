void HAL_LCD_PortInit(void)
{
    //slas789c -> p.98
    P1SEL1 &= ~BIT4;
    P1SEL0 |= BIT4;

    //slas789c -> p.98
    P1SEL1 &= ~BIT6;
    P1SEL0 |= BIT6;

    //reset
    //general output, used as reset in lcd_driver, set to 11
    // to prevent parasitic cross currents when applying analog signals
    P9DIR |= BIT4;
    P9OUT |= BIT4;

    //data
    // slas789c -> p.16
    //USCI_A0: Slave transmit enable (SPI mode)
    P2DIR |= BIT3;

    //chip select
    // slas789c -> p.101
    P2DIR |= BIT4;
    P2OUT &= ~BIT4; // Enabled (Low)

    // Backlight (P2.6) - Required for the MKII display to be visible
    /*
    P2DIR |= BIT6;
    P2OUT |= BIT6;*/

    // Unlock GPIO
    PM5CTL0 &= ~LOCKLPM5;

    return;
}

void HAL_LCD_SpiInit(void)
{
    //slau367o -> 816
    // Put eUSCI in reset
    UCB0CTLW0 = UCSWRST;

    // (UCCKPH = 0)Set clock phase to "capture on 1st edge, change on following edge"
    // (UCCKPL = 0)Set clock polarity to "inactive low"
    // (UCMSB = 1)Set data order to "transmit MSB first"
    // (UC7BIT = 0)Set data size to 8-bit
    // (UCMST = 1)Set MCU to "SPI master"
    // (UCMODE_0)Set SPI to "3-pin SPI" (we won't use eUSCI's chip select)
    // (UCSYNC= 1) Set module to synchronous mode
    // (UCSSE_2) Set clock to SMCLK
    UCB0CTLW0 |= UCCKPH | UCMSB | UCMST | UCMODE_0 | UCSYNC | UCSSEL_2;
    //UCB0CTLW0 |= UCCKPL| UCCKPH | ~(UCMSB) | ~(UCMST) | UCMODE_0 | ~(UCSYNC) | UCSSEL_2| UC7BIT ;

    // Configure the clock divider (SMCLK is set to 16 MHz; run SPI at 8 MHz using SMCLK)
    // Maximum SPI supported frequency on the display is 10 MHz
    // Clock divider: 16MHz / 2 = 8MHz (Stay under 10MHz limit)
    //slau367o -> 779
    UCB0BRW = 2;

    // Exit reset
    UCB0CTLW0 &= ~UCSWRST;

    // Set CS' (chip select) bit to 0 (display always enabled)
    // Set DC' bit to 0 (assume data)
    // Default pin states
    P2OUT &= ~BIT4; // CS Low
    P2OUT &= ~BIT3; // DC Low

    return;
}
