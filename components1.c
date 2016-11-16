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

void transmit_data(unsigned short data) {
	// for each bit of data
	unsigned char i;
	unsigned char SRCLR = 0, CLK_1 = 0, CLK_2 = 0, SER = 0;
	unsigned char msb;
	SER = 0;
	CLK_1 = 2;
	SRCLR = 1;
	CLK_2 = 3;
	// Set SRCLR to 1 allowing data to be set
	PORTC = SetBit(PORTC,SRCLR,1);
	for (i=0;i<16;i++)
	{
		if(data & (1 << (15-i)))
		{
			msb = 1;
		}
		else
		{
			msb = 0;
		}
		// Also clear SRCLK in preparation of sending data
		PORTC = SetBit(PORTC,CLK_1,0);
		PORTC = SetBit(PORTC,CLK_2,0);
		// set SER = next bit of data to be sent.
		PORTC = SetBit(PORTC,SER,msb);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTC = SetBit(PORTC,CLK_1,1);
		PORTC = SetBit(PORTC,CLK_2,1);
		// end for each bit of data
	}
	// clears all lines in preparation of a new transmission
	PORTC = SetBit(PORTC,CLK_1,0);
	PORTC = SetBit(PORTC,CLK_2,0);
	PORTC = SetBit(PORTC,CLK_1,1);
	PORTC = SetBit(PORTC,CLK_2,1);
	PORTC = SetBit(PORTC,SRCLR,0);
}


unsigned char pinADC = 0x00;
unsigned short force1_input = 0;
unsigned short force2_input = 0;
unsigned short light_input = 0;
unsigned char reset = 0;
unsigned short debug_output = 0;

// The following code is for DEBUGGING
enum LEDState {on, off} led_state;

void LEDS_Init(){
	led_state = off;
}

void LEDS_Tick(){
	Set_A2D_Pin(pinADC);
	_delay_ms(5);
	debug_output = ADC;
	//Actions
	switch(led_state){
		case off:
			transmit_data(0x0000);
		break;
		case on:
			transmit_data(debug_output);
		break;
		default:
			transmit_data(0x0000);
		break;
	}
	//Transitions
	switch(led_state){
		case off:
			led_state = on;
		break;
		case on:
			led_state = on;
		break;
		default:
			led_state = off;
		break;
	}
}

void LedSecTask()
{
	LEDS_Init();
   for(;;) 
   { 	
	LEDS_Tick();
	vTaskDelay(50); 
   } 
}

void StartSecPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(LedSecTask, (signed portCHAR *)"LedSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}	

enum ButtonState {start, check, force, light} button_state;

void Button_Init(){
	button_state = start;
}

void Button_Tick(){
	//Actions
	switch(button_state){
		case start:
			pinADC = 0x00;
		break;
		case check:
		break;
		case force:
			pinADC = 0x00;
			reset = 0;
		break;
		case light:
			pinADC = 0x01;
			reset = 1;
		break;
		default:
			pinADC = 0x00;
		break;
	}
	//Transitions
	switch(button_state){
		case start:
			button_state = force;
		break;
		case check:
			if(!GetBit(PINB, 1))
			{
				button_state = force;
			}
			else if (!GetBit(PINB, 2))
			{
				button_state = light;
			}
			else
			{
				button_state = check;
			}
		break;
		case force:
			button_state = check;
		break;
		case light:
			button_state = check;
		break;
		default:
			button_state = start;
		break;
	}
}

void ButtonSecTask()
{
	Button_Init();
	for(;;)
	{
		Button_Tick();
		vTaskDelay(50);
	}
}

void StartButtonPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(ButtonSecTask, (signed portCHAR *)"ButtonSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

// End of DEBUGGING code

enum FSRState {check_force, increment_force, finish_force, reset_force} force_state;
unsigned short force_cnt = 0;
unsigned short force_low_range = 0x0180;
unsigned short force_high_range = 0x0200;

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
			PORTD = SetBit(PORTD, 0, 0);
			force_cnt = 0;
		break;
		case increment_force:
			force_cnt++;
		break;
		case finish_force:
			PORTD = SetBit(PORTD, 0, 1);
		break;
		case reset_force:
			force_cnt = 0;
		break;
		default:
			PORTD = SetBit(PORTD, 0, 0);
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
			if (reset == 1)
			{
				force_state = reset_force;
			} 
			else
			{
				force_state = finish_force;
			}
		break;
		case reset_force:
			if (reset == 0)
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
			PORTD = SetBit(PORTD, 1, 0);
		break;
		case correct_light:
			PORTD = SetBit(PORTD, 1, 1);
		break;
		default:
			PORTD = SetBit(PORTD, 1, 0);
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
   DDRB = 0x00; PORTB = 0xFF;
   DDRC = 0xFF;
   DDRD = 0xFF; PORTD = 0x00;
   //Start Tasks  
   StartSecPulse(1);
   StartButtonPulse(1);
   StartFSRPulse(1);
   StartLightPulse(1);
    //RunSchedular 
   vTaskStartScheduler(); 
 
   return 0; 
}