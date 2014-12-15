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
unsigned short photoValueD2;
unsigned short photoValueU2;

/* flags for gestures */
bool leftRight = false;
bool rightLeft = false;
bool downUp = false;
bool upDown = false;

/* direction flags */
unsigned char lr = 0x01;
unsigned char rl = 0x02;
unsigned char du = 0x03;
unsigned char ud = 0x04;
unsigned char du2 = 0x05;
unsigned char ud2 = 0x06;

/* calibrates the sensor that is passed in */
unsigned short calibrate(unsigned char resistor) {
	Set_A2D_Pin(resistor);
	for(int i = 0; i < 100; ++i);
	unsigned short avg = 0;
	for(int i = 0; i < 10; ++i) avg += ADC;
	return avg / 10;
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

/* check the value of a specific resistor like at 0x02 (pin 2)
   and return whether it senses a shadow or not */ 
bool checkPhotoValue(unsigned char resistor) {
	unsigned short photoValue;
	switch(resistor) {
		case 0x02:
			photoValue = photoValueL;
			break;
		case 0x00:
			photoValue = photoValueU;
			break;
		case 0x04:
			photoValue = photoValueR;
			break;
		case 0x01:
			photoValue = photoValueD;
			break;
		case 0x05:
			photoValue = photoValueU2;
			break;
		case 0x06:
			photoValue = photoValueD2;
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

void key_Tick() {
	unsigned char input = GetKeypadKey();
	if(input == 'A') {
		input = ' ';
	} else if(input == 'B') {
		input = '.';
	} else if(input == '#') {
		photoValueL = calibrate(0x02) - 4;
		photoValueR = calibrate(0x04) - 4;
		photoValueD = calibrate(0x01) - 4;
		photoValueU = calibrate(0x00) - 4;
		photoValueD2 = calibrate(0x06) - 4;
		photoValueU2 = calibrate(0x05) - 4;
	}
	if(input != '\0') {
		sendDir(input);
	}
}

void sendDir(unsigned char sendValue) {
	if(USART_IsSendReady(1)) {
		USART_Send(sendValue, 1);
	}
}

void keyTask() {
	for(;;) { 	
		key_Tick();
		vTaskDelay(100); 
	} 
}

enum LRState {initlr, l0, l1, l2} lr_state;
enum RLState {initrl, r0, r1, r2} rl_state;
enum DUState {initdu, d0, d1, d2} du_state;
enum UDState {initud, u0, u1, u2} ud_state;
//enum DU2State {initdu2, d20, d21, d22} du2_state;
//enum UD2State {initud2, u20, u21, u22} ud2_state;

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
/*
void du2_Init(){
	du2_state = initdu2;
}

void ud2_Init(){
	ud2_state = initud2;
}
*/
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
			} else if((!checkPhotoValue(0x02) && checkPhotoValue(0x04))) {
			lr_state = l2;
			sendDir(lr);
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
		//PORTC = 0x00;
		break;
		case l2:
		//PORTC = 0x01;
		break;
	}
}

void rl_Tick(){
	switch(rl_state) {
		case initrl:
		rl_state = r0;
		break;
		case r0:
		if(checkPhotoValue(0x04) && !checkPhotoValue(0x02)) {
			rl_state = r1;
		}
		break;
		case r1:
		if(!checkPhotoValue(0x04) && !checkPhotoValue(0x02)) {
			rl_state = r0;
			} else if((!checkPhotoValue(0x04) && checkPhotoValue(0x02))) {
			rl_state = r2;
			sendDir(rl);
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
		//PORTC = 0x00;
		break;
		case r2:
		//PORTC = 0x02;
		break;
	}
}

void du_Tick(){
	switch(du_state) {
		case initdu:
		du_state = d0;
		break;
		case d0:
		if(checkPhotoValue(0x01) && !checkPhotoValue(0x00)) {
			du_state = d1;
		}
		break;
		case d1:
		if(!checkPhotoValue(0x01) && !checkPhotoValue(0x00)) {
			du_state = d0;
			} else if(!checkPhotoValue(0x01) && checkPhotoValue(0x00)) {
			du_state = d2;
			sendDir(du);
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
		PORTC = 0x00;
		break;
		case d2:
		PORTC = 0x04;
		break;
	}
}

void ud_Tick(){
	switch(ud_state) {
		case initud:
		ud_state = u0;
		break;
		case u0:
		if(checkPhotoValue(0x00) && !checkPhotoValue(0x01)) {
			ud_state = u1;
		}
		break;
		case u1:
		if(!checkPhotoValue(0x00) && !checkPhotoValue(0x01)) {
			ud_state = u0;
			} else if(!checkPhotoValue(0x00) && checkPhotoValue(0x01)) {
			ud_state = u2;
			sendDir(ud);
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
		PORTC = 0x00;
		break;
		case u2:
		PORTC = 0x08;
		break;
	}
}
/*
void du2_Tick(){
	switch(du2_state) {
		case initdu2:
		du2_state = d20;
		break;
		case d20:
		if(checkPhotoValue(0x06) && !checkPhotoValue(0x05)) {
			du2_state = d21;
		}
		break;
		case d21:
		if(!checkPhotoValue(0x06) && !checkPhotoValue(0x05)) {
			du2_state = d20;
			} else if(!checkPhotoValue(0x06) && checkPhotoValue(0x05)) {
			du2_state = d22;
			sendDir(du2);
			} else {
			du2_state = d21;
		}
		break;
		case d22:
		du2_state = d20;
		break;
		default:
		du2_state = initdu2;
		break;
	}
	switch(du2_state) {
		case d20:
		//PORTC = 0x00;
		break;
		case d22:
		//PORTC = 0x04;
		break;
	}
}

void ud2_Tick(){
	switch(ud2_state) {
		case initud2:
		ud2_state = u20;
		break;
		case u20:
		if(checkPhotoValue(0x05) && !checkPhotoValue(0x06)) {
			ud2_state = u21;
		}
		break;
		case u21:
		if(!checkPhotoValue(0x05) && !checkPhotoValue(0x06)) {
			ud2_state = u20;
			} else if(!checkPhotoValue(0x05) && checkPhotoValue(0x06)) {
			ud2_state = u22;
			sendDir(ud2);
			} else {
			ud2_state = u21;
		}
		break;
		case u22:
		ud2_state = u20;
		break;
		default:
		ud2_state = initud2;
		break;
	}
	switch(ud2_state) {
		case u20:
		//PORTC = 0x00;
		break;
		case u22:
		//PORTC = 0x08;
		break;
	}
}
*/
void lrTask()
{
	lr_Init();
	for(;;) {
		lr_Tick();
		vTaskDelay(50);
	}
}

void rlTask()
{
	rl_Init();
	for(;;) {
		rl_Tick();
		vTaskDelay(50);
	}
}

void duTask()
{
	du_Init();
	for(;;) {
		du_Tick();
		vTaskDelay(50);
	}
}

void udTask()
{
	ud_Init();
	for(;;) {
		ud_Tick();
		vTaskDelay(50);
	}
}
/*
void du2Task()
{
	du2_Init();
	for(;;) {
		du2_Tick();
		vTaskDelay(50);
	}
}

void ud2Task()
{
	ud2_Init();
	for(;;) {
		ud2_Tick();
		vTaskDelay(50);
	}
}
*/
void keyPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(keyTask, (signed portCHAR *)"keyTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
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
/*
void Startdu2(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(du2Task, (signed portCHAR *)"du2Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

void Startud2(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(ud2Task, (signed portCHAR *)"ud2Task", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
 */
int main(void) 
{ 
   DDRC = 0xFF; PORTC = 0x00;
   DDRB = 0xF0; PORTB = 0x0F;
   
   
   //Initialize components and registers
   ADC_init();
   photoValueL = calibrate(0x02) - 4;
   photoValueR = calibrate(0x04) - 4;
   photoValueD = calibrate(0x01) - 4;
   photoValueU = calibrate(0x00) - 4;
   //photoValueD2 = calibrate(0x06) - 4;
   //photoValueU2 = calibrate(0x05) - 4;
   initUSART(1);
   
   //Start Tasks  
   keyPulse(1);
   Startlr(1);
   Startrl(1);
   Startdu(1);
   Startud(1);
   //Startdu2(1);
   //Startud2(1);
	//RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}