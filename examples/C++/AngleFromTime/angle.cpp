/*****************************************************************************************************
*
*	Name : angle.cpp
*
*   Description : Angle class implementation
*
*	By : Wayne J. Dupont
*
*	Date : 08/06/2013
*
******************************************************************************************************/	
#include "angle.h"
#include "clocktime.h"

int CAngle::getAngle(CClockTime ct)
{
	// Get the hour and minute from the clocktime class
	int hour = ct.getHour();
	int minute = ct.getMinute();

	// If we are using a of 24 hour clock
	if(hour > 12) 
	{
		hour -= 12; 
	}

	// Calculate the time for the given time
	angle =  hour * 30 - minute * 6; 

	// Calculate the angle if it is greater than 180 degrees
	if(angle > 180) 
	{
		angle = 360 - angle; 
	}

	// Return the angle
	return angle;
};