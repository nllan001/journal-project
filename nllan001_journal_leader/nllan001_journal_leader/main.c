#include <stdint.h> 
#include <stdlib.h> 
#include <stdio.h> 
#include <stdbool.h> 
#include <string.h> 
#include <math.h> 
#include <avr/io.h> 
#include <avr/interrupt.h> 
#include <avr/eeprom.h> 
#include <avr/portpins.h> 
#include <avr/pgmspace.h> 
 
//FreeRTOS include files 
#include "FreeRTOS.h" 
#include "task.h" 
#include "croutine.h" 

//Include given files
#include "bit.h"
#include "keypad.h"
#include "lcd.h"
#include "usart_ATmega1284.h"

/* constants for different thresholds concerning ac values */
const unsigned short photoValue = 120;
const unsigned short joystickLRLeft = 504 + 50;
const unsigned short joystickLRRight = 504 - 50;
const unsigned short joystickUDDown = 504 - 50;
const unsigned short joystickUDUp = 504 + 50;

/* the array of entries for the journal */
char entries[5][100];

/* fills entries for checking purposes */
void fillEntries() {
	strcpy(entries[0], "hello");
	strcpy(entries[1], "goodbye");
}

/* check the value of a specific resistor like at 0x02 (pin 2)
   and return whether it senses a shadow or not */ 
bool checkPhotoValue(unsigned char resistor) {
	Set_A2D_Pin(resistor);
	for(int i = 0; i < 100; ++i);
	unsigned short input = ADC;
	/* For debugging purposes. It outputs the ADC value to the LCD. */
	/*
	char value[16];
	sprintf(value, "%u", input);
	LCD_DisplayString(1, value);
	*/
	if(input < photoValue) {
		return true;
	} else {
		return false;
	}
}

/* check the direction that the joystick is being pushed. 
   pass in l, r, d, or u for left, right, down, or up to check
   those specific directions on the joystick */
bool checkDirection(const char direction) {
	if(direction == 'd' || direction == 'u') {
		Set_A2D_Pin(0x00);
		for(int i = 0; i < 100; ++i);
		unsigned short input = ADC;
		if(direction == 'd') {
			return input < joystickUDDown ? true : false;
		} else {
			return input > joystickUDUp ? true : false;
		}	
	} else if(direction == 'l' || direction == 'r') {
		Set_A2D_Pin(0x01);
		for(int i = 0; i < 100; ++i);
		unsigned short input = ADC;
		if(direction == 'l') {
			return input > joystickLRLeft ? true : false;
			} else {
			return input < joystickLRRight ? true : false;
		}
	} else {
		return false;
	}
}

/* initialize the ADC register for checking analog values */
void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}

/* set which pin is being used as Vin to compare against the Vref from the AREF pin */
void Set_A2D_Pin(unsigned char pinNum) {
	ADMUX = (pinNum <= 0x07) ? pinNum : ADMUX;
	// Allow channel to stabilize
	static unsigned char i = 0;
	for ( i=0; i<15; i++ ) { asm("nop"); }
}

enum LEDState {INIT,L0,L1,L2,L3,L4,L5,L6,L7} led_state;

void LEDS_Init(){
	led_state = INIT;
}

void LEDS_Tick(){
	unsigned short input = ADC;
	/* For debugging purposes. It outputs the ADC value to the LCD. */
	char value[16];
	sprintf(value, "%u", input);
	LCD_DisplayString(1, value);
	PORTD = checkDirection('d') ? 0x80 : 0x00;
}

void LedSecTask()
{
	LEDS_Init();
   for(;;) 
   { 	
	LEDS_Tick();
	vTaskDelay(500); 
   } 
}

void StartSecPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(LedSecTask, (signed portCHAR *)"LedSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}	
 
int main(void) 
{ 
   //DDRA = 0x00; PORTA = 0xFF;
   DDRD = 0xFF; PORTD = 0x00;
   DDRB = 0xFF; PORTB = 0x00;
   
   //Initialize components and registers
   LCD_init();
   ADC_init();
   
   fillEntries();
   //Start Tasks  
   StartSecPulse(1);
    //RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}