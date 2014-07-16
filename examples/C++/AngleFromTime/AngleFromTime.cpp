/*****************************************************************************************************
*
*	Name : AngleFromTime.cpp
*
*   Description : Console application that calculates the angle between the hour hand and the minute
*	hand for a given time.
*	
*	By : Wayne J. Dupont
*
*	Date : 08/06/2013
*
******************************************************************************************************/

#include <iostream>
#include <string>
#include <ctype.h>
#include "clocktime.h"
#include "angle.h"

using namespace std;

int main()
{
	int hr;
	int min;
	int a;
	char c;

	// Create our class instances
	CClockTime ctime;
	CAngle angle;

	// Loop until the user want to quit
	do 
	{

		cout << "\n\nProgram to calculate the angle between the hour and minute hands on\n";
		cout << "for a given time.\n\n";

		// Get the hour 
		cout << "Enter the hour : ";
		cin >> hr;

		// Store the hour to our instance
		ctime.setHour(hr);

		// Get the minute
		cout << "Enter Minute : ";
		cin >> min;

		// Store the minute to our instance
		ctime.setMinute(min);

		// Pass in our time object and calculate the angle
		a = angle.getAngle(ctime);

		// Display the result 
		// Use abs() to be sure return a positive number for the
		// angle.
		printf("\n\nThe angle is : %d degrees\n\n", abs(a));

		// Aske the user if they want to contine
		printf("\n\nPress 'Y' to continue : ");
		cin >> c;

		// Clear the screen 
		cout << string(100, '\n');

	}while(toupper(c) == 'Y');
}