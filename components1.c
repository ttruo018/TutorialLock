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
#include <util/delay.h>
 
//FreeRTOS include files 
#include "FreeRTOS.h" 
#include "task.h" 
#include "croutine.h" 
#include "bit.h"

void A2D_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	// ADEN: Enables analog-to-digital conversion
	// ADSC: Starts analog-to-digital conversion
	// ADATE: Enables auto-triggering, allowing for constant
	//	    analog to digital conversions.
}

// Pins on PORTA are used as input for A2D conversion
//    The default channel is 0 (PA0)
// The value of pinNum determines the pin on PORTA
//    used for A2D conversion
// Valid values range between 0 and 7, where the value
//    represents the desired pin for A2D conversion
void Set_A2D_Pin(unsigned char pinNum) {
	ADMUX = (pinNum <= 0x07) ? pinNum : ADMUX;
	// Allow channel to stabilize
	static unsigned char i = 0;
	for ( i=0; i<15; i++ ) { asm("nop"); }
}

unsigned short force1_input = 0;
unsigned short force2_input = 0;
unsigned short light_input = 0;
unsigned char status = 0x03;
unsigned char receivedData = 0;

// Servant code
void SPI_ServantInit(void) {
	// set DDRB to have MISO line as output and MOSI, SCK, and SS as input
	DDRB = 0x40;
	PORTB = 0xBF;
	// set SPCR register to enable SPI and enable SPI interrupt (pg. 168)
	SPCR = 1<<6 | 1<<7;
	// make sure global interrupts are enabled on SREG register (pg. 9)
	//SREG = 1<<7;
	sei();
}

ISR(SPI_STC_vect) { // this is enabled in with the SPCR register
	// Interrupt Enable	// SPDR contains the received data, e.g. unsigned char receivedData =
	// SPDR;
	receivedData = SPDR;
	SPDR = status;
}

// The following code is for DEBUGGING
// End of DEBUGGING code

enum FSRState {check_force, increment_force, finish_force, reset_force} force_state;
unsigned short force_cnt = 0;
unsigned short force_low_range = 0x0200;
unsigned short force_high_range = 0x0300;

void FSR_Init(){
	force_state = check_force;
}

void FSR_Tick(){
	Set_A2D_Pin(0x00);
	_delay_ms(5);
	force1_input = ADC;
	Set_A2D_Pin(0x01);
	_delay_ms(5);
	force2_input = ADC;
	//Actions
	switch(force_state){
		case check_force:
			PORTC = SetBit(PORTC, 0, 0);
			force_cnt = 0;
			status |= 0x01;
		break;
		case increment_force:
			force_cnt++;
		break;
		case finish_force:
			PORTC = SetBit(PORTC, 0, 1);
			status &= 0xFE;
		break;
		case reset_force:
			force_cnt = 0;
		break;
		default:
			PORTC = SetBit(PORTC, 0, 0);
		break;
	}
	//Transitions
	switch(force_state){
		case check_force:
			if (force1_input < force_high_range && force1_input > force_low_range && force2_input < force_high_range && force2_input > force_low_range)
			{
				force_state = increment_force;
			}
			else
			{
				force_state = check_force;
			}
		break;
		case increment_force:
			if (force1_input < 0x0200 && force1_input > 0x0180 && force2_input < force_high_range && force2_input > force_low_range)
			{
				if (force_cnt > 500)
				{
					force_state = finish_force;
				}
				else
				{
					force_state = increment_force;
				}
			} 
			else
			{
				force_state = check_force;
			}
		case finish_force:
			if (receivedData == 0x80)
			{
				force_state = reset_force;
			} 
			else
			{
				force_state = finish_force;
			}
		break;
		case reset_force:
			if (receivedData == 0x01)
			{
				force_state = check_force;
			} 
			else
			{
				force_state = reset_force;
			}
		break;
		default:
			force_state = check_force;
		break;
	}
}

void FSRSecTask()
{
	FSR_Init();
	for(;;)
	{
		FSR_Tick();
		vTaskDelay(50);
	}
}

void StartFSRPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(FSRSecTask, (signed portCHAR *)"FSRSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

enum LightState {wrong_light, correct_light} light_state;

void Light_Init(){
	light_state = wrong_light;
}

void Light_Tick(){
	Set_A2D_Pin(0x02);
	_delay_ms(5);
	light_input = ADC;
	//Actions
	switch(light_state){
		case wrong_light:
			PORTC = SetBit(PORTC, 1, 0);
			status |= 0x02;
		break;
		case correct_light:
			PORTC = SetBit(PORTC, 1, 1);
			status &= 0xFD;
		break;
		default:
			PORTC = SetBit(PORTC, 1, 0);
		break;
	}
	//Transitions
	switch(light_state){
		case wrong_light:
			if (light_input < 0x0010)
			{
				light_state = correct_light;
			}
			else
			{
				light_state = wrong_light;
			}
		break;
		case correct_light:
			if (light_input < 0x0010)
			{
				light_state = correct_light;
			}
			else
			{
				light_state = wrong_light;
			}
		break;
		default:
		light_state = wrong_light;
		break;
	}
}

void LightSecTask()
{
	Light_Init();
	for(;;)
	{
		Light_Tick();
		vTaskDelay(50);
	}
}

void StartLightPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(LightSecTask, (signed portCHAR *)"LightSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}
 
int main(void) 
{ 
   A2D_init();
   DDRC = 0xFF;
   DDRD = 0x00; PORTD = 0xFF;
   SPI_ServantInit();
   //Start Tasks  
   StartFSRPulse(1);
   StartLightPulse(1);
    //RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}