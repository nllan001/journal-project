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
unsigned short photoValueL;
unsigned short photoValueR;
unsigned short photoValueD;
unsigned short photoValueU;
const unsigned short joystickLRLeft = 504 + 100;
const unsigned short joystickLRRight = 504 - 100;
const unsigned short joystickUDDown = 504 - 150;
const unsigned short joystickUDUp = 504 + 150;

/* flag for menu */
int location = 1;
bool sleep = false;

/* direction flags */
bool lr = false;
bool rl = false;
bool du = false;
bool ud = false;
bool du2 = false;
bool ud2 = false;

unsigned char receive;

unsigned char cursorPos = 1;
const unsigned char screenWidth = 16;
const int numEntries = 10;

/* the array of entries for the journal */
char entries[10][100];

/* the menu */
char menu[1][100];

/* fills entries for checking purposes */
void fillEntries() {
	strcpy(menu[0], "Journal Menu:   v Entries");
	strcpy(entries[0], "Entry 1");
	strcpy(entries[1], "Entry 2");
	strcpy(entries[2], "Entry 3");
	strcpy(entries[3], "Entry 4");
	strcpy(entries[4], "Entry 5");
	strcpy(entries[5], "Entry 6");
	strcpy(entries[6], "Entry 7");
	strcpy(entries[7], "Entry 8");
	strcpy(entries[8], "Entry 9");
	strcpy(entries[9], "Entry 10");
}

/* calibrates the sensor that is passed in */
unsigned short calibrate(unsigned char resistor) {
	Set_A2D_Pin(resistor);
	for(int i = 0; i < 100; ++i);
	unsigned short avg = 0;
	for(int i = 0; i < 10; ++i) avg += ADC;
	return avg / 10;
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
	unsigned short photoValue;
	switch(resistor) {
		case 0x02:
			photoValue = photoValueL;
			break;
		case 0x03:
			photoValue = photoValueU;
			break;
		case 0x04:
			photoValue = photoValueR;
			break;
		case 0x05:
			photoValue = photoValueD;
			break;
		default:
			return false;
			break;
	}
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
	unsigned char input = receive;
	if(input != '\0' && input != 0x01 && input != 0x02 && input != 0x03 && input != 0x04 && input != 0x05 && input != 0x06) {
		receive = '\0';
		if(input != '#' && input != 0x01 && input != 0x02 && input != 0x03 && input != 0x04 && input != 0x05 && input != 0x06) {
			entries[currentEntry][cursorPos - 1] = input;
			if(cursorPos < 32) cursorPos++;
		} else if(input == '#') {
			photoValueL = calibrate(0x02) - 2;
			photoValueR = calibrate(0x04) - 2;
			photoValueD = calibrate(0x05) - 2;
			photoValueU = calibrate(0x03) - 2;
		}
	}
	if(sleep) {
		LCD_DisplayString(1, "");
		LCD_Cursor(1);
	} else if(location == 1) {
		LCD_DisplayString(1, menu);
		LCD_Cursor(1);
	} else if(location == 2) {
		LCD_DisplayString(1, entries[currentEntry]);
		LCD_Cursor(cursorPos);
	}
	
}

/* change current entry depending on gestures performed */
void changeEntry() {
	if(rl) {
		if(currentEntry < numEntries - 1) {
			currentEntry++;
			cursorPos = 1;
		}
	} else if(lr) {
		if(currentEntry > 0) {
			currentEntry--;
			cursorPos = 1;
		}
	} else if(ud) {
		if(location > 1) {
			location--;
		}
	} else if(du) {
		if(location < 2) {
			location++;
		}
	} /*else if(ud2) {
		sleep = true;
	} else if(du2) {
		sleep = false;
	} */
}

/* check usart 0 for flags */
void checkU() {
	unsigned char check = receive;
	if((lr = (check == 0x01)) || (rl = (check == 0x02)) || (du = (check == 0x03)) || (ud = (check == 0x04)) || (du2 = (check == 0x05)) || (ud2 = (check == 0x06))) {
		receive = '\0';
	}
	if(check == 0x03) PORTC = 0x01;
	else if(check == 0x04) PORTC = 0x02;
	else if(check == 0x01) PORTC = 0x03;
	else if(check == 0x02) PORTC = 0x04;
	else PORTC = 0x00;
}

void TextTask() {
	for(;;) {
		enterText();
		changeEntry();
		checkU();
		vTaskDelay(150);
	}
}

void ReceiveTask() {
	for(;;) {
		if(USART_HasReceived(1)) {
			receive = USART_Receive(1);
			USART_Flush(1);
		}
		vTaskDelay(50);
	}
}

void StartText(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(TextTask, (signed portCHAR *)"TextTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void StartRec(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(ReceiveTask, (signed portCHAR *)"ReceiveTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
 
int main(void) 
{ 
   DDRB = 0xFF; PORTB = 0x00;
   DDRD = 0xFA; PORTD = 0x05;
   DDRC = 0xFF; PORTC = 0x00;
   
   //Initialize components and registers
   LCD_init();
   ADC_init();
   //photoValueL = calibrate(0x02) - 2;
   //photoValueR = calibrate(0x04) - 2;
   initUSART(1);
   
   fillEntries();
   //Start Tasks  
   //Startlr(1);
   //Startrl(1);
   StartRec(1);
   StartText(1);
    //RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}