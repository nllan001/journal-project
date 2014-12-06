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

enum LEDState {INIT,L0,L1,L2,L3,L4,L5,L6,L7} led_state;

void LEDS_Init(){
	led_state = INIT;
}

void LEDS_Tick(){
	unsigned char input = GetKeypadKey();
	if(input == 'A') {
		PORTC = 0x01;
	} else {
		PORTC = 0x00;
	}
	if(USART_IsSendReady(1)) {
		USART_Send(GetKeypadKey(), 1);
	}
	if(USART_HasTransmitted(1)) {
		USART_Flush(1);
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
		if(checkPhotoValue(0x02)) {
			lr_state = l1;
		}
		break;
		case l1:
		if(!checkPhotoValue(0x02) && !checkPhotoValue(0x04)) {
			lr_state = l0;
			} else if(!checkPhotoValue(0x02) && checkPhotoValue(0x04)) {
			lr_state = l2;
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
		if(checkPhotoValue(0x04)) {
			rl_state = r1;
		}
		break;
		case r1:
		if(!checkPhotoValue(0x04) && !checkPhotoValue(0x02)) {
			rl_state = r0;
			} else if(!checkPhotoValue(0x04) && checkPhotoValue(0x02)) {
			rl_state = r2;
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
		if(checkPhotoValue(0x05)) {
			du_state = d1;
		}
		break;
		case d1:
		if(!checkPhotoValue(0x05) && !checkPhotoValue(0x03)) {
			du_state = d0;
			} else if(!checkPhotoValue(0x05) && checkPhotoValue(0x03)) {
			du_state = d2;
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

void lrTask()
{
	lr_Init();
	for(;;) {
		lr_Tick();
		vTaskDelay(150);
	}
}

void rlTask()
{
	rl_Init();
	for(;;) {
		rl_Tick();
		vTaskDelay(150);
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

void StartSecPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(LedSecTask, (signed portCHAR *)"LedSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
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
   //DDRA = 0x00; PORTA = 0xFF;
   DDRC = 0xFF; PORTC = 0x00;
   DDRB = 0xF0; PORTB = 0x0F;
   //DDRD = 0x5F; PORTD = 0xA0;
   
   
   //Initialize components and registers
   ADC_init();
   initUSART(0);
   initUSART(1);
   
   //Start Tasks  
   StartSecPulse(1);
    //RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}