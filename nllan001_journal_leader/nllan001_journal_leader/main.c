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

/* flags for gestures */
bool leftRight = false;
bool rightLeft = false;
bool downUp = false;
bool upDown = false;

unsigned char cursorPos = 1;
const unsigned char screenWidth = 16;
const int numEntries = 10;

/* the array of entries for the journal */
char entries[10][100];

/* the menu */
char menu[1][100];

/* fills entries for checking purposes */
void fillEntries() {
	strcpy(menu[0], "Main Menu:      Sensors^vEntries");
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
	unsigned char input = USART_Receive(1);
	USART_Flush(1);
	if(input != '\0') {
		if(input != '#') {
			entries[currentEntry][cursorPos - 1] = input;
			if(cursorPos < 32) cursorPos++;
		} else {
			photoValueL = calibrate(0x02) - 4;
			photoValueR = calibrate(0x04) - 4;
			photoValueD = calibrate(0x05) - 4;
			photoValueU = calibrate(0x03) - 4;
			PORTC = 0x10;
		}
	}
	LCD_DisplayString(1, entries[currentEntry]);
	LCD_Cursor(cursorPos);
}

/* change current entry depending on gestures performed */
void changeEntry() {
	if(rightLeft) {
		if(currentEntry < numEntries - 1) {
			currentEntry++;
			cursorPos = 1;
		}
	} else if(leftRight) {
		if(currentEntry > 0) {
			currentEntry--;
			cursorPos = 1;
		}
	}
}

/* The following series of state machines check for gestures
   performed above the photo resistors */

enum LRState {initlr, l0, l1, l2} lr_state;
enum RLState {initrl, r0, r1, r2} rl_state;
enum DUState {initdu, d0, d1, d2} du_state;
enum UDState {initud, u0, u1, u2} ud_state;

void lr_Init(){
	lr_state = initlr;
}

void rl_Init(){
	rl_state = initrl;
}

void du_Init(){
	du_state = initdu;
}

void ud_Init(){
	ud_state = initud;
}

void lr_Tick(){
	switch(lr_state) {
		case initlr:
		lr_state = l0;
		break;
		case l0:
		if(checkPhotoValue(0x02) && !checkPhotoValue(0x04)) {
			lr_state = l1;
		}
		break;
		case l1:
		if(!checkPhotoValue(0x02) && !checkPhotoValue(0x04)) {
				lr_state = l0;
		} else if((!checkPhotoValue(0x02) && checkPhotoValue(0x04)) || (checkPhotoValue(0x02) && (checkPhotoValue(0x04)))) {
				lr_state = l2;
				leftRight = true;
		} else {
				lr_state = l1;
		}
		break;
		case l2:
		lr_state = l0;
		break;
		default:
		lr_state = initlr;
		break;
	}
	switch(lr_state) {
		case l0:
		leftRight = false;
		PORTC = 0x00;
		break;
		case l2:
		PORTC = 0x02;
		break;
	}
}

void rl_Tick(){
	switch(rl_state) {
		case initrl:
		rl_state = r0;
		break;
		case r0:
		if(checkPhotoValue(0x04)) {
			rl_state = r1;
		}
		break;
		case r1:
		if(!checkPhotoValue(0x04) && !checkPhotoValue(0x02)) {
			rl_state = r0;
		} else if((!checkPhotoValue(0x04) && checkPhotoValue(0x02))) {
				rl_state = r2;
				rightLeft = true;
		} else {
				rl_state = r1;
		}
		break;
		case r2:
		rl_state = r0;
		break;
		default:
		rl_state = initrl;
		break;
	}
	switch(rl_state) {
		case r0:
		PORTC = 0x00;
		rightLeft = false;
		break;
		case r2:
		PORTC = 0x01;
		break;
	}
}

void du_Tick(){
	switch(du_state) {
		case initdu:
		du_state = d0;
		break;
		case d0:
		if(checkPhotoValue(0x05)) {
			du_state = d1;
		}
		break;
		case d1:
		if(!checkPhotoValue(0x05) && !checkPhotoValue(0x03)) {
			du_state = d0;
			} else if(!checkPhotoValue(0x05) && checkPhotoValue(0x03)) {
			du_state = d2;
			downUp = true;
			} else {
			du_state = d1;
		}
		break;
		case d2:
		du_state = d0;
		break;
		default:
		du_state = initdu;
		break;
	}
	switch(du_state) {
		case d0:
		downUp = false;
		//PORTC = 0x00;
		break;
		case d2:
		//PORTC = 0x04;
		break;
	}
}

void ud_Tick(){
	switch(ud_state) {
		case initud:
		ud_state = u0;
		break;
		case u0:
		if(checkPhotoValue(0x03)) {
			ud_state = u1;
		}
		break;
		case u1:
		if(!checkPhotoValue(0x03) && !checkPhotoValue(0x05)) {
			ud_state = u0;
			} else if(!checkPhotoValue(0x03) && checkPhotoValue(0x05)) {
			ud_state = u2;
			upDown = true;
			} else {
			ud_state = u1;
		}
		break;
		case u2:
		ud_state = u0;
		break;
		default:
		ud_state = initud;
		break;
	}
	switch(ud_state) {
		case u0:
		upDown = false;
		//PORTC = 0x00;
		break;
		case u2:
		//PORTC = 0x08;
		break;
	}
}

void lrTask()
{
	lr_Init();
	for(;;) { 	
		lr_Tick();
		vTaskDelay(100); 
	} 
}

void rlTask()
{
	rl_Init();
	for(;;) {
		rl_Tick();
		vTaskDelay(100);
	}
}

void duTask()
{
	du_Init();
	for(;;) {
		du_Tick();
		vTaskDelay(150);
	}
}

void udTask()
{
	ud_Init();
	for(;;) {
		ud_Tick();
		vTaskDelay(150);
	}
}

void TextTask() {
	for(;;) {
/*
	Set_A2D_Pin(0x02);
	for(int i=0;i<100;++i);
	unsigned short input1 = ADC;
	Set_A2D_Pin(0x04);
	for(int i=0;i<100;++i);
	unsigned short input2 = ADC;
	char value1[16], value2[16];
	sprintf(value1, "%u", input1);
	sprintf(value2, "%u", input2);
	strcat(value1, " ");
	strcat(value1, value2);
	LCD_DisplayString(1, value1);
*/
/*
	char value[16];
	sprintf(value, "%c", GetKeypadKey());
	LCD_DisplayString(1, value);
*/
		
		enterText();
		changeEntry();
		vTaskDelay(100);
	}
}

void Startlr(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(lrTask, (signed portCHAR *)"lrTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void Startrl(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(rlTask, (signed portCHAR *)"rlTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void Startdu(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(duTask, (signed portCHAR *)"duTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void Startud(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(udTask, (signed portCHAR *)"udTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
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
   photoValueL = calibrate(0x02) - 4;
   photoValueR = calibrate(0x04) - 4;
   photoValueD = calibrate(0x05) - 4;
   photoValueU = calibrate(0x03) - 4;
   initUSART(0);
   initUSART(1);
   
   //emptyEntries();
   fillEntries();
   //Start Tasks  
   Startlr(1);
   Startrl(1);
   Startdu(1);
   Startud(1);
   StartText(1);
    //RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}