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
			PORTB = SetBit(PORTB, 1, 0);
			motion_cnt = 0;
		break;
		case wait_motion:
			PORTB = SetBit(PORTB, 1, 0);
		break;
		case update_motion:
			motion_cnt++;
			PORTB = SetBit(PORTB, 1, 1);
		break;
		case reset_motion:
			PORTB = SetBit(PORTB, 1, 0);
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
			PORTB = SetBit(PORTB, 0, 0);
			active_motion = 0;
		break;
		case on_PIR:
			PORTB = SetBit(PORTB, 0, 1);
			active_motion = 1;
		break;
		default:
			PORTB = SetBit(PORTB, 0, 0);
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

// Debugging
// enum KeyState {init_Key, update_Key} Key_state;
// unsigned char key = ' ';
// 
// void Key_Init()
// {
// 	Key_state = init_Key;
// }
// 
// void Key_Tick()
// {
// 	key = GetKeypadKey();
// 	//Actions
// 	switch(Key_state) {
// 		case init_Key:
// 			transmit_data(0x00F0);
// 		break;
// 		case update_Key:
// 			if (key == '1')
// 			{
// 				transmit_data(0x0001);
// 			} 
// 			else if (key == '2')
// 			{
// 				transmit_data(0x0002);
// 			}
// 			else if (key == '3')
// 			{
// 				transmit_data(0x0003);
// 			}
// 			else if (key == '4')
// 			{
// 				transmit_data(0x0004);
// 			}
// 			else if (key == '5')
// 			{
// 				transmit_data(0x0005);
// 			}
// 			else if (key == '6')
// 			{
// 				transmit_data(0x0006);
// 			}
// 			else if (key == '7')
// 			{
// 				transmit_data(0x0007);
// 			}
// 			else if (key == '8')
// 			{
// 				transmit_data(0x0008);
// 			}
// 			else if (key == '9')
// 			{
// 				transmit_data(0x0009);
// 			}
// 			else if (key == 'A')
// 			{
// 				transmit_data(0x000A);
// 			}
// 			else if (key == 'B')
// 			{
// 				transmit_data(0x000B);
// 			}
// 			else if (key == 'C')
// 			{
// 				transmit_data(0x000C);
// 			}
// 			else if (key == 'D')
// 			{
// 				transmit_data(0x000D);
// 			}
// 			else if (key == '*')
// 			{
// 				transmit_data(0x000E);
// 			}
// 			else if (key == '0')
// 			{
// 				transmit_data(0x000F);
// 			}
// 			else if (key == '#')
// 			{
// 				transmit_data(0x0010);
// 			}
// 		break;
// 		default:
// 			PORTB = SetBit(PORTB, 0, 0);
// 		break;
// 	}
// 	//Transitions
// 	switch(Key_state){
// 		case init_Key:
// 			Key_state = update_Key;
// 		break;
// 		case update_Key:
// 			Key_state = update_Key;
// 		break;
// 		default:
// 			Key_state = init_Key;
// 		break;
// 	}
// }
// 
// void KeyTask()
// {
// 	Key_Init();
// 	for (;;)
// 	{
// 		Key_Tick();
// 		vTaskDelay(100);
// 	}
// }
// 
// void StartKeyPulse(unsigned portBASE_TYPE Priority)
// {
// 	xTaskCreate(KeyTask, (signed portCHAR *)"KeyTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
// }
// End of Debugging

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
			PORTB = SetBit(PORTB, 2, 0);
		break;
		case check_Key:
			if (key == passcode[passNum])
			{
				key_complete++;
			}
			passNum++;
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
			PORTB = SetBit(PORTB, 2, 1);
			key_open = 0;
		break;
		default:
			PORTB = SetBit(PORTB, 2, 0);
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
			if (key == '#')
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
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xF0; PORTD = 0x0F;
	//Start Tasks
	StartMotionPulse(1);
	StartPIRPulse(1);
	StartKeyPulse(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}