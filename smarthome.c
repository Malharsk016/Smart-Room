#include <lpc214x.h>
#include <stdio.h>  // for sprintf function
#include <stdlib.h> // for atoi function
#include <string.h>
#include "timerdelay.h"
#include "lcd.h"
#include "keypad.h"

#define LCD (0xFFFF00FF)
#define RS (1<<4)
#define RW (1<<5)
#define EN (1<<6)
int ADC_val = 0, prev = 0, check = 0, temp_f = 0, TEMP_LIMIT = 29; // Initialize variables
char val[10];	
void ADC_INIT(void);	     // Initial setup of ADC
int ADC(void);		     // Returns ADC Value
int UART(void);		     // UART interface
void LCD_INIT(void);   	     // Initial setup of LCD
void LCD_CMD(char command);  // Instruction to LCD
void LCD_STRING (char* msg); // String to LCD
void delay_ms(int count);    // 1 milli second delay

#define bit(x) (1 << x)

char flag = 0;
char pwd[] = "3333";

void lock(void);
void ledTurnOn(void); // Function to turn on LED
void therometer_check(void);
	
int main(void)
{ 
    char key[5];
    int cons = 0;
    lcd_initial();
    lcd_cmd(0x01);
    lcd_string("Press 1 to change");
    lcd_cmd(0x01);
    lcd_string("new password");

    flag = get_key();

    if (flag == '1')
    {
        lcd_cmd(0x01);
        lcd_string("New password:");
        for (cons = 0; cons < 4; cons++)
        {
            pwd[cons] = get_key();
            lcd_string("*");
        }
        pwd[cons] = 0;
    }

    while (1)
    {
        lcd_cmd(0x01);
        lcd_string("Password:");
        
        for (cons = 0; cons < 4; cons++)
        {
            key[cons] = get_key();
            lcd_string("*");
        }
        key[cons] = 0;

        if (strcmp(key, pwd) != 0)
        {
            lcd_cmd(0x20);
            lcd_cmd(0x01);
            lcd_string("Wrong Password!!");
        }
        else
        {
           lock();
        }
    }
		
}

void lock(void)
{
    IO0DIR |= bit(19) | bit(21);
    lcd_cmd(0x01);
    lcd_string("LOCK IS OPENING");
    ledTurnOn(); // Turn on LED when door opens
    IO0SET |= bit(19);
    timemdel(50);
    IO0CLR |= bit(19);
    timemdel(100);
    lcd_cmd(0x01);
    lcd_string("LOCK IS CLOSING");
    IO0SET |= bit(21);
    timemdel(50);
    IO0CLR |= bit(21);
	  IO0DIR |= bit(16); // Set P0.16 as output
	  PINSEL0 = 0x00000000;
	  IODIR0 |= 0X00100000;  // P0.20 as output for DC Fan
	  IODIR0 &= 0XFFFEFFFF;  // P0.16 as input from switch
	  ADC_INIT();  
	  LCD_INIT(); 
	  therometer_check();
	  
}

void ledTurnOn(void)
{
    // Assuming the LED is connected to Port0 Pin 16 (P0.16)
    IO0SET |= bit(16); // Turn on the LED
}

void ADC_INIT(void)
  {
	PINSEL1 &= 0xFF7FFFFF; // (PINSEL1<23> = 0)
	PINSEL1 |= 0x00400000; // (PINSEL1<22> = 1)
	//P0.27 is Configured as ADC Pin AD0.0
	PCONP |= (1<<12);      // Enable Power/Clock to ADC0
  }
void LCD_INIT(void)
{
	//P0.12 to P0.15 LCD Data. P0.4,5,6 as RS RW and EN 
	IO0DIR |= 0x0000F070; 
	delay_ms(20);	
	LCD_CMD(0x02);  // Initialize cursor to home position
	LCD_CMD(0x28);  // 4 - bit interface, 2 line, 5x8 dots 
	LCD_CMD(0x06);  // Auto increment cursor 
	LCD_CMD(0x0C);  // Display on cursor off 
	LCD_CMD(0x01);  // Display clear 
	LCD_CMD(0x80);  // First line first position 
}

void therometer_check(void)
{
	while (1)
	{
		ADC_val = ADC(); // Returns ADC Value
		if ((IOPIN0 &  0x00010000) == 0x00010000) // if P0.16 is high
		{
			TEMP_LIMIT = UART(); // Returns temperature limit from PC
			check = 1; //executes if statement to check again  
		}
		delay_ms(1000); // 1000 milli sec = 1 sec
		if (prev != ADC_val || check == 1)
		{
			temp_f = (ADC_val *9/5)+32 ; // temperature in farenheit
			sprintf(val," %dC / %dF",ADC_val,temp_f); // int to string
				 
			LCD_CMD(0x01);  // Display clear 
			LCD_STRING(val);
			LCD_CMD(0xC0);	// New line in display		 
				
			if (ADC_val >= TEMP_LIMIT)
			{
				IOSET0 |= 0x00100000; // sets P0.20 for relay
				LCD_STRING(" OverHeat");
			}
			else
			{
				IOCLR0 |= 0x00100000; // clears P0.16 for relay
				LCD_STRING(" Normal");
			}
			prev = ADC_val;
		}
		check = 0;
	}
}

int ADC(void)
  {
  	unsigned int ADC_data;
	AD0CR = 0x00200700; //CLKDIV=(PCLK)/8, BURST=0, CLKS=11clks/10bits, PDN=1
	AD0CR|= 0x01;       // A/D Channel 0
	AD0CR |= (1<<24);   //Activate ADC Module (new conversion )
	//Wait for conversion to get over by monitoring 28th bit of A/D register
	while(!(AD0GDR & 0x80000000));  
	//Read 10 bit ADC Data ie RESULT = 10 Bit Data (15:6)
	ADC_data = (AD0GDR >> 6)& 0x3FF; 
	//Deactivate ADC Module ie START = 000 (Bits 26:24) (stop conversion)
	AD0CR &= 0xF8FFFFFF; 

	return ADC_data;
  }

	
int UART(void)
  {
  	unsigned int i=0 ,val;
	char data = 0, str[4];
	PINSEL0 |= 0x00000005; // Enable RxD0 and TxD0
	U0LCR = 0x83;   // 8 bits, no parity , 1 stop bits, DLAB = 1
	U0DLL = 97;     // 9600 Baud Rate @ 15MHZ VPB Clock 
	U0LCR = 0x03;   // DLAB = 0 
	while(data != 44) // if data received is not comma
         {
	  	// Wait until reception is over and UART0 is ready with data
		while(!(U0LSR & 0x01)); 
		data = U0RBR;  //Receive character
		// Wait until UART0 ready to send character
		while(!(U0LSR & 0x20)); 
		U0THR = data;  //Send character
		str[i] = data;
		i++;
	  }
	val = atoi(str); //converts string into integer
	return val;
}

void LCD_CMD(char command)
{
	IO0PIN = ( (IO0PIN & LCD) | ((command & 0xF0)<<8) ); //Upper nibble 
	IO0SET = EN; // EN = 1 
	IO0CLR = (RS|RW); // RS = 0, RW = 0 
	delay_ms(5); // 5 milli sec
	IO0CLR = EN; // EN = 0, RS and RW unchanged(RS = RW = 0)	 
	delay_ms(5);
	IO0PIN = ( (IO0PIN & LCD) | ((command & 0x0F)<<12) ); //Lower nibble 
	IO0SET = EN; // EN = 1 
	IO0CLR = (RS|RW); // RS = 0, RW = 0 
	delay_ms(5);
	IO0CLR = EN; // EN = 0, RS and RW unchanged(RS = RW = 0)	 
	delay_ms(5);	
}


void LCD_STRING (char* msg)
{
	unsigned int i=0;
	while(msg[i]!=0)
	{
		IO0PIN = ( (IO0PIN & LCD) | ((msg[i] & 0xF0)<<8) ); //Upper nibble
		IO0SET = (RS|EN); // RS = 1, EN = 1 
		IO0CLR = RW; // RW = 0 
		delay_ms(2); // 20 milli sec
		IO0CLR = EN; // EN = 0, RS and RW unchanged(RS = 1, RW = 0) 
		delay_ms(5);
		IO0PIN = ( (IO0PIN & LCD) | ((msg[i] & 0x0F)<<12) ); //lower nibble
		IO0SET = (RS|EN); // RS = 1, EN = 1 
		IO0CLR =  RW; // RW = 0 
		delay_ms(2);
		IO0CLR = EN; // EN = 0, RS and RW unchanged(RS = 1, RW = 0) 
		delay_ms(5);
		i++;
	}
}


void delay_ms(int count)
{
  int j=0,i=0;
  for(j=0;j<count;j++)
  {
    // At 60Mhz, the below loop introduces delay of 1 milli sec 
    for(i=0;i<12500;i++);
  }
}
