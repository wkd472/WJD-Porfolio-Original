/*****************************************************************************************************
*
*	Name : clocktime.cpp
*
*   Description : Clocktime class implementation
*
*	By : Wayne J. Dupont
*
*	Date : 08/06/2013
*
******************************************************************************************************/	
#include "clocktime.h"

	// Function to set the hour
	void CClockTime::setHour(int h)
	{
		hour = h;
	}

	// Function to get the hour
	int CClockTime::getHour()
	{
		return hour;
	}

	// Function to set the minute
	void CClockTime::setMinute(int m)
	{
		minute = m;
	}

	// Function to get the minute
	int CClockTime::getMinute()
	{
		return minute;
	}