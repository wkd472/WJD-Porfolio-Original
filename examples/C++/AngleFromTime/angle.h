/*****************************************************************************************************
*
*	Name : angle.cpp
*
*   Description : Class that calculates and returns the angle for a given clock time
*
*	By : Wayne J. Dupont
*
*	Date : 08/06/2013
*
******************************************************************************************************/
// Forward referemnce to this class
class CClockTime;

// Define the angle class
class CAngle
{
	int angle;

public :

	int getAngle(CClockTime ct);
};
