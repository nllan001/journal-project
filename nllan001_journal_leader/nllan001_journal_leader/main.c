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
const unsigned short photoValue = 40;
const unsigned short joystickLRLeft = 504 + 100;
const unsigned short joystickLRRight = 504 - 100;
const unsigned short joystickUDDown = 504 - 150;
const unsigned short joystickUDUp = 504 + 150;

unsigned char cursorPos = 1;
const unsigned char screenWidth = 16;
const int numEntries = 5;

/* the array of entries for the journal */
char entries[5][100];

/* fills entries for checking purposes */
void fillEntries() {
	strcpy(entries[0], "hello");
	strcpy(entries[1], "goodbye");
}

/* track the currently displayed entry in the array */
unsigned char currentEntry = 0;

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

/* check the value of a specific resistor like at 0x02 (pin 2)
   and return whether it senses a shadow or not */ 
bool checkPhotoValue(unsigned char resistor) {
	Set_A2D_Pin(resistor);
	for(int i = 0; i < 100; ++i);
	unsigned short input = ADC;
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

/* check for movements from the joystick to move the cursor */
void moveCursor() {
	if(checkDirection('r')) {
		if(cursorPos < screenWidth * 2) {
			cursorPos++;
		}
	} else if(checkDirection('l')) {
		if(cursorPos > 1) {
			cursorPos--;
		}
	} else if(checkDirection('u')) {
		if(cursorPos > screenWidth) {
			cursorPos -= screenWidth;
		}
	} else if(checkDirection('d')) {
		if(cursorPos <= screenWidth) {
			cursorPos += screenWidth;
		}
	}
	LCD_Cursor(cursorPos);
}

/* allow for movement on the screen and check for input.
   if there is input, replace the text at the current position.
   once usart is added, make sure getkeypadkey is replaced with
   listening for input from the follower. it's possible i might have
   to move the moveCursor out of the function. */
void enterText() {
		moveCursor();
		//unsigned char input = GetKeypadKey();
		unsigned char input = USART_Receive(1);
		USART_Flush(1);
		if(input != '\0') {
			entries[currentEntry][cursorPos - 1] = input;
			if(cursorPos < 32) cursorPos++;
		}
		LCD_DisplayString(1, entries[currentEntry]);
		LCD_Cursor(cursorPos);
}

enum LEDState {INIT,L0,L1,L2,L3,L4,L5,L6,L7} led_state;

void LEDS_Init(){
	led_state = INIT;
}

void LEDS_Tick(){
	switch(led_state) {
		case(INIT):
		led_state = L0;
		break;
		case(L0):
		if(checkPhotoValue(0x02) && !checkPhotoValue(0x04)) {
			led_state = L1;
		}
		break;
		case(L1):
		if(!checkPhotoValue(0x02) && !checkPhotoValue(0x04)) {
			led_state = L0;
		} else if(!checkPhotoValue(0x02) && checkPhotoValue(0x04)) {
			led_state = L2;
		} else {
			led_state = L1;
		}
		break;
		case(L2):
		led_state = L0;
		break;
		default:
		led_state = INIT;
		break;
	}
	switch(led_state) {
		case(INIT):
		break;
		case(L0):
		PORTC = 0x00;
		break;
		case(L1):
		break;
		case(L2):
		PORTC = 0x01;
		break;
	}
}

void LedSecTask()
{
	LEDS_Init();
   for(;;) 
   { 	
	LEDS_Tick();
	vTaskDelay(100); 
   } 
}

void TextTask() {
	for(;;) {
/*	Set_A2D_Pin(0x02);
	for(int i=0;i<100;++i);
	unsigned short input = ADC;
	char value[16];
	sprintf(value, "%u", input);
	LCD_DisplayString(1, value);
*/
	//PORTD = checkDirection('d') ? 0x80 : 0x00;*/
/*
	char value[16];
	sprintf(value, "%c", GetKeypadKey());
	LCD_DisplayString(1, value);
*/
		enterText();
		vTaskDelay(200);
	}
}

void StartSecPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(LedSecTask, (signed portCHAR *)"LedSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}	

void StartText(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(TextTask, (signed portCHAR *)"TextTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
 
int main(void) 
{ 
   DDRB = 0xFF; PORTB = 0x00;
   DDRD = 0xFA; PORTD = 0x05;
   DDRC = 0xFF; PORTC = 0x00;
   
   //Initialize components and registers
   LCD_init();
   ADC_init();
   initUSART(0);
   initUSART(1);
   
   //emptyEntries();
   fillEntries();
   //Start Tasks  
   StartText(1);
   StartSecPulse(1);
    //RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}