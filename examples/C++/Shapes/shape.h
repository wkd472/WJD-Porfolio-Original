/*
*	Define a Shape class
*
*	Wayne J. Dupont
*
*   August 7, 2013
*/
#ifndef SHAPE_H
#define SHAPE_H

#include <string>
using namespace std;

class Shapes
{

public:

	Shapes();

	// Set the name
	void setName(string n);

	// Get the name
	string getName();

	// Set the area
	void setArea(double a);

	// Get the area
	double getArea();

	// Set the parimeter
	void setPerimeter(double a);

	// Get the perimeter
	double getPerimeter();

private:

	string name;
	double area;
	double perimeter;
};
#endif
