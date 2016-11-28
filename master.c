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

unsigned char direction;
unsigned char motor_output;
unsigned char motor_complete;
unsigned int phases;
unsigned int degree;
unsigned short period = 5;

enum MotorState {motor_init, A, AB, B, BC, C, CD, D, DA} motor_state;
void Motor_Init(){
	motor_state = motor_init;
}

void Motor_Tick(){
	//Actions
	switch(motor_state){
		case motor_init:
			motor_output = 0;
		break;
		case A:
			motor_output = 0x01;
			phases--;
		break;
		case AB:
			motor_output = 0x03;
			phases--;
		break;
		case B:
			motor_output = 0x02;
			phases--;
		break;
		case BC:
			motor_output = 0x06;
			phases--;
		break;
		case C:
			motor_output = 0x04;
			phases--;
		break;
		case CD:
			motor_output = 0x0C;
			phases--;
		break;
		case D:
			motor_output = 0x08;
			phases--;
		break;
		case DA:
			motor_output = 0x09;
			phases--;
		break;
		default:
		break;
	}
	//Transitions
	switch(motor_state){
		case motor_init:
		motor_state = A;
		break;
		case A:
			if (direction == 1)
			{
				motor_state = AB;
			}
			else if (direction == 2)
			{
				motor_state = DA;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		case AB:
			if (direction == 1)
			{
				motor_state = B;
			}
			else if (direction == 2)
			{
				motor_state = A;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		case B:
			if (direction == 1)
			{
				motor_state = BC;
			}
			else if (direction == 2)
			{
				motor_state = AB;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		case BC:
			if (direction == 1)
			{
				motor_state = C;
			}
			else if (direction == 2)
			{
				motor_state = B;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		case C:
			if (direction == 1)
			{
				motor_state = CD;
			}
			else if (direction == 2)
			{
				motor_state = BC;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		case CD:
			if (direction == 1)
			{
				motor_state = D;
			}
			else if (direction == 2)
			{
				motor_state = C;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		case D:
			if (direction == 1)
			{
				motor_state = DA;
			}
			else if (direction == 2)
			{
				motor_state = CD;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		case DA:
			if (direction == 1)
			{
				motor_state = A;
			}
			else if (direction == 2)
			{
				motor_state = D;
			}
			if (phases == 0)
			{
				motor_complete = 1;
			}
		break;
		default:
			motor_state = motor_init;
		break;
	}
	PORTA = motor_output;
}

void MotorSecTask()
{
	Motor_Init();
	for(;;)
	{
		Motor_Tick();
		vTaskDelay(period);
	}
}

void StartMotorPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(MotorSecTask, (signed portCHAR *)"MotorSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

enum ControlState {control_init, lock, unlock, opening, open, reset, closing} control_state;
void Control_Init() {
	control_state = control_init;
}

void Control_Tick(){
	//Actions
	switch(control_state){
		case control_init:
			degree = 135;
			direction = 0;
			motor_output = 0;
			phases = 0;
			motor_complete = 0;
		break;
		case lock:
			direction = 0;
		break;
		case unlock:
			direction = 1;
			phases = (degree / 5.625) * 64;
		break;
		case opening:
		break;
		case open:
			direction = 0;
		break;
		case reset:
			direction = 2;
			phases = (degree / 5.625) * 64;
		break;
		default:
		break;
	}
	//Transitions
	switch(control_state){
		case control_init:
			control_state = lock;
		break;
		case lock:
			if (!GetBit(PIND, 0))
			{
				control_state = unlock;
			}
			else
			{
				control_state = lock;
			}
		break;
		case unlock:
			control_state = opening;
		break;
		case opening:
			if (motor_complete == 1)
			{
				motor_complete = 0;
				control_state = open;
			}
		break;
		case open:
			if (!GetBit(PIND, 1))
			{
				control_state = reset;
			}
			else
			{
				control_state = open;
			}
		break;
		case reset:
			control_state = closing;
		break;
		case closing:
			if (motor_complete == 1)
			{
				motor_complete = 0;
				control_state = lock;
			}
		break;
		default:
			control_state = control_init;
		break;
	}
}

void ControlTask()
{
	Control_Init();
	for(;;)
	{
		Control_Tick();
		vTaskDelay(period);
	}
}

void StartControlPulse(unsigned portBASE_TYPE Priority)
{
	xTaskCreate(ControlTask, (signed portCHAR *)"ControlTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(void)
{
	DDRA = 0xFF;
	DDRD = 0x00; PORTD = 0xFF;
	//Start Tasks
	StartMotorPulse(1);
	StartControlPulse(1);
	//RunSchedular
	vTaskStartScheduler();
	
	return 0;
}