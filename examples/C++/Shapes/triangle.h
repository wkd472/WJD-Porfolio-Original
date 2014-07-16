/*
*	Define a Triangle class
*
*	Wayne J. Dupont
*
*   August 7, 2013
*/
#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "shape.h"

class Triangle: public Shapes
{
public:
	Triangle(double s1, double s2, double s3);

private:

	double side1;
	double side2;
	double side3;

	double calcArea();
	double calcPerimeter();
};

#endif
