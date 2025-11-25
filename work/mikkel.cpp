#include <Arduino.h>
#include "i2c.h"
#include "ssd1306.h"
#include <msp430.h>

// i sætter kald af initialiserings funktioner fra opgave 4 kode ind over while(1)
volatile char t_flag1 = 0, t_flag2 = 0;

const float SCALER = 48.0;

unsigned int captured_value1 = 0;
unsigned int captured_value2 = 0;
float freq1 = 0, freq2 = 0;
float freqMax = 525.0;

void init_SMCLK_25MHz()
{
    WDTCTL = WDTPW | WDTHOLD; // Stop the watchdog timer

    P5SEL |= BIT2 + BIT3; // Select XT2 for SMCLK (Pins 5.2 and 5.3)
    P5SEL |= BIT4 + BIT5;
    // Configure DCO to 25 MHz
    __bis_SR_register(SCG0); // Disable FLL control loop
    UCSCTL0 = 0x0000;        // Set lowest possible DCOx and MODx
    UCSCTL1 = DCORSEL_7;     // Select DCO range (DCORSEL_7 for max range)
    UCSCTL2 = FLLD_0 + 610;  // FLLD = 1, Multiplier N = 762 for ~25 MHz DCO   - 610 for 20Mhz
    // calculated by f DCOCLK  =32.768kHz×610=20MHz
    __bic_SR_register(SCG0); // Enable FLL control loop

    // Loop until XT2, XT1, and DCO stabilize
    do
    {
        UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG); // Clear fault flags
        SFRIFG1 &= ~OFIFG;                          // Clear oscillator fault flags
    } while (SFRIFG1 & OFIFG); // Wait until stable

    UCSCTL3 = SELREF__REFOCLK;                            // Set FLL reference to REFO
    UCSCTL4 = SELA__XT1CLK | SELS__DCOCLK | SELM__DCOCLK; // Set ACLK = XT1; SMCLK = DCO; MCLK = DCO
    UCSCTL5 = DIVS__1;                                    // Set SMCLK divider to 1 (no division)
}

void init_pwm()
{
    WDTCTL = WDTPW | WDTHOLD; // whatcdog disabel

    // TA1CTL bruges til at sige at vi indstiler timer A1 . *TASSEL_2* spisificere at vi bruger SMKL (1.048 MHz)
    // *MC_3* siger at vi tæller up til *TA1CCR0* og ned til 0, *ID_0* siger at vi skal dividere clokens Hz med 1
    TA1CTL |= TASSEL_2 | MC_3 | ID_0;

    // difindere hvad tællren skal tælle til
    TA1CCR0 = 1024;

    // PWM fq = 19.988 / (2*TA1CCR0) = næsten 10 kH

    // hvad der skal tælles til for at tænde for output.
    // configureres til en % del af *TA1CCR0* (idt. 30% resultrende i en 70% uptime)
    TA1CCR1 = 512;

    // sætter output mode. mode 3 resæter når *TA1CCR0* resætter
    TA1CCTL1 = OUTMOD_2;

    // sæter P2_0 til out put (der hvor SMCLK's PWM kommder ud)
    P2DIR |= BIT0;
    P2SEL |= BIT0;
}

void init_capture()
{
    TA0CTL |= TASSEL_1 | MC_2 | ID_0;

    //
    TA0CCTL1 |= CM_1 | CAP | CCIS_0 | CCIE;

    //
    TA0CCTL2 |= CM_1 | CAP | CCIS_0 | CCIE;

    // CM_1 = er capture mode sat til capture on rising edge (1)
    // CAP = er capture mode. bliver sat til når den bliver nævnt
    // CCIS_0 = input tul cpa/com. sat til CCIxA
    // CCIE = cpature compare intrupt enabel

    P1DIR |= ~(BIT2 + BIT3);
    P1SEL |= BIT2 | BIT3;
}

int main()
{
    WDTCTL = WDTPW | WDTHOLD; // always disable watch for using delay function

    init_SMCLK_25MHz(); // set 20 MHz for SMCLK

    // inisialisere PWM
    init_pwm();

    // inisialisere kollen(x) til capture line
    init_capture();

    // initialiserings af OLED
    __delay_cycles(100000);
    i2c_init();

    __delay_cycles(100000);
    ssd1306_init();

    __delay_cycles(1000000);
    reset_display();

    __delay_cycles(1000000);
    // ssd1306_printText(0, 0, "hej med dig/0");

    unsigned int Xd = 0;

    __enable_interrupt();
    float Gm = 1;
    float Gin = 0;

    float Xe, Xf;
    int error = 0;
    float G = 1;
    float adc_res_av = 0;
    int k = 0, m = 0, n = 0;
    char data[12];

    float duty = 0;
    float RPS_1 = 0;
    float RPS_2 = 0;

    // formatet print variabler

    char duty_print[32] = {};
    char freq_print[32] = {};
    // char freq1_print[32] = {};
    // char freq2_print[32] = {};
    char RPM1_print[32] = {};
    char RPM2_print[32] = {};
    char temp[32] = {};

    while (1)
    {
        duty = (float)(TA1CCR1 * 100) / TA1CCR0;
        dtostrf(duty, 0, 2, temp);
        sprintf(duty_print, "%s%%", temp);
        ssd1306_printText(0, 0, duty_print);

        if (t_flag1)
        {
            t_flag1 = 0;

            // freq1_av = (unsigned int)freq1 / 48;

            // print af frekvens
            // dtostrf(freq1 / (2 * 48), 0, 2, freq1_print);
            // dtostrf(freq1, 0, 2, freq1_print);
            // sprintf(freq1_print, "%s", freq1_print);
            // ssd1306_printText(0, 1, freq1_print);

            // print af RPS
            RPS_1 = freq1 / SCALER;
            dtostrf(RPS_1, 0, 2, temp);
            sprintf(RPM1_print, "%s", temp);
            ssd1306_printText(0, 1, RPM1_print);
        }
        if (t_flag2)
        {
            t_flag2 = 0;

            // print af frekvens
            // dtostrf(freq2 / (2 * 48), 0, 2, freq2_print);
            // sprintf(freq2_print, "%s", freq2_print);
            // ssd1306_printText(0, 3, freq2_print);

            // print af RPS
            RPS_2 = freq2/SCALER;
            dtostrf(RPS_2 , 0, 2, temp);
            sprintf(RPM2_print, "%s", temp);
            ssd1306_printText(0, 2, RPM2_print);
        }
    }
}

#pragma vector = TIMER0_A1_VECTOR
__interrupt void Timer_ISR(void)
{
    static unsigned int last1 = 0;
    static int i = 0, n = 0;
    static unsigned int last2 = 0;
    switch (TA0IV)
    {
    case 0x02: // CCR1  P1.2
               // Handle the captured value for the first encoder pulse

        // P2OUT ^= BIT2;

        if (last1 > TA0CCR1)
        {
            captured_value1 = 65535 - last1 + TA0CCR1;
        }
        else
        {
            captured_value1 = (TA0CCR1 - last1);
        }
        last1 = TA0CCR1;
        i++;
        if (i == 2)
        {
            freq1 = (float)(32768.0 / captured_value1);

            captured_value1 = 0;
            i = 0;
            t_flag1 = 1;
        }

        break;
    case 0x04: // CCR2 (P1.3)
               // Handle the captured value for the second encoder pulse
        if (last2 > TA0CCR2)
        {
            captured_value2 = 65535 - last2 + TA0CCR2;
        }
        else
        {
            captured_value2 = (TA0CCR2 - last2);
        }
        // P2OUT ^= BIT3;
        last2 = TA0CCR2;
        n++;
        if (n == 2)
        {
            freq2 = (float)(32768.0 / captured_value2);
            t_flag2 = 1;
            captured_value2 = 0;
            n = 0;
        }
        break;

    default:
        break;
    }
}