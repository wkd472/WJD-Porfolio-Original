/*****************************************************************************************************
*
*	Name : clocktime.h
*
*   Description : Class that holds the given clock time
*
*	By : Wayne J. Dupont
*
*	Date : 08/06/2013
*
******************************************************************************************************/

// Define clocktime class
class CClockTime
{
	// Variables to hold the time
	int hour;			
	int minute;

public :

	// Function to set the hour
	void setHour(int h);

	// Function to get the hour
	int getHour();

	// Function to set the minute
	void setMinute(int m);

	// Function to get the minute
	int getMinute();

	// Define the friend class
	friend class CAngle;
};