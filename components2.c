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
#include "keypad.h"

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

enum MotionState {init_motion, wait_motion, update_motion, reset_motion} motion_state;
unsigned short motion_cnt = 0;
unsigned char active_motion = 0;

void Motion_Init()
{
	motion_state = init_motion;
}

void Motion_Tick()
{
	//Actions
	switch(motion_state) {
		case init_motion:
			PORTC = SetBit(PORTC, 1, 0);
			motion_cnt = 0;
			status = status | 0x01;
		break;
		case wait_motion:
			PORTC = SetBit(PORTC, 1, 0);
			status = status & 0xFE;
		break;
		case update_motion:
			motion_cnt++;
			PORTC = SetBit(PORTC, 1, 1);
		break;
		case reset_motion:
			PORTC = SetBit(PORTC, 1, 0);
			motion_cnt = 0;
		break;
		default:
			motion_cnt = 0;
		break;
	}
	//Transitions
	switch(motion_state){
		case init_motion:
			if (active_motion == 1)
			{
				motion_state = wait_motion;
			}
			else
			{
				motion_state = init_motion;
			}
		break;
		case wait_motion:
			if (active_motion == 0)
			{
				motion_state = update_motion;
			}
			else
			{
				motion_state = wait_motion;
			}
		break;
		case update_motion:
			if (motion_cnt > 1000)
			{
				motion_state = reset_motion;
			}
			else
			{
				motion_state = update_motion;
			}
		break;
		case reset_motion:
			motion_state = init_motion;
		break;
		default:
			motion_state = init_motion;
		break;
	}
}

void MotionTask()
{
	Motion_Init();
	for (;;)
	{
		Motion_Tick();
		vTaskDelay(10);
	}
}

void StartMotionPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(MotionTask, (signed portCHAR *)"MotionTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}


enum PIRState {off_PIR, on_PIR} PIR_state;

void PIR_Init()
{
	PIR_state = off_PIR;
}

void PIR_Tick()
{
	//Actions
	switch(PIR_state) {
		case off_PIR:
			PORTC = SetBit(PORTC, 0, 0);
			active_motion = 0;
		break;
		case on_PIR:
			PORTC = SetBit(PORTC, 0, 1);
			active_motion = 1;
		break;
		default:
			PORTC = SetBit(PORTC, 0, 0);
		break;
	}
	//Transitions
	switch(PIR_state){
		case off_PIR:
			if (GetBit(PINA, 0))
			{
				PIR_state = on_PIR;
			}
			else
			{
				PIR_state = off_PIR;
			}
		break;
		case on_PIR:
			if (!GetBit(PINA, 0))
			{
				PIR_state = off_PIR;
			}
			else
			{
				PIR_state = on_PIR;
			}
			break;
		default:
			PIR_state = off_PIR;
		break;
	}
}

void PIRTask()
{
	PIR_Init();
	for (;;)
	{
		PIR_Tick();
		vTaskDelay(10);
	}
}

void StartPIRPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(PIRTask, (signed portCHAR *)"PIRTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

enum KeyState {lock_Key, check_Key, compare_Key, unlock_Key} Key_state;
unsigned char key = ' ';
unsigned char passcode[4] = {'1', '2', '3', '4'};
unsigned char passNum = 0;
unsigned char key_complete = 0;
unsigned char key_open = 0;

void Key_Init()
{
	Key_state = lock_Key;
}

void Key_Tick()
{
	//Actions
	switch(Key_state) {
		case lock_Key:
			PORTC = SetBit(PORTC, 2, 0);
			PORTC = SetBit(PORTC, 3, 0);
			status |= 0x02;
		break;
		case check_Key:
			if (key == passcode[passNum])
			{
				key_complete++;
			}
			passNum++;
			PORTC = SetBit(PORTC, 2, 1);
			_delay_ms(50);
		break;
		case compare_Key:
			passNum = 0;
			if (key_complete == 4)
			{
				key_open = 1;
			}
			key_complete = 0;
		break;
		case unlock_Key:
			key_open = 0;
			status &= 0xFD;
			PORTC = SetBit(PORTC, 3, 1);
		break;
		default:
			PORTC = SetBit(PORTC, 2, 0);
			PORTC = SetBit(PORTC, 3, 0);
		break;
	}
	key = GetKeypadKey();
	//Transitions
	switch(Key_state){
		case lock_Key:
			if (key != '\0')
			{
				Key_state = check_Key;
			}
			else if (passNum == 4)
			{
				Key_state = compare_Key;
			}
			else
			{
				Key_state = lock_Key;
			}
		break;
		case check_Key:
			Key_state = lock_Key;
		break;
		case compare_Key:
			if (key_open == 1)
			{
				Key_state = unlock_Key;
			}
			else
			{
				Key_state = lock_Key;
			}
		break;
		case unlock_Key:
			if (receivedData == 0x80)
			{
				Key_state = lock_Key;
			}
			else
			{
				Key_state = unlock_Key;
			}
		break;
		default:
		Key_state = lock_Key;
		break;
	}
}

void KeyTask()
{
	Key_Init();
	for (;;)
	{
		Key_Tick();
		vTaskDelay(50);
	}
}

void StartKeyPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(KeyTask, (signed portCHAR *)"KeyTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(void)
{
	DDRA = 0x00; PORTA = 0xFF;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xF0; PORTD = 0x0F;
	SPI_ServantInit();
	//Start Tasks
	StartMotionPulse(1);
	StartPIRPulse(1);
	StartKeyPulse(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}