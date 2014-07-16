#define _USE_MATH_DEFINES 
#include <cmath>
#include "shape.h"
/*
*	Circle class implimetation
*
*	Wayne J. Dupont
*
*   August 7, 2013
*/
#include "shape.h"
#include "circle.h"

Circle::Circle(double a): Shapes()
{
	setName("Circle");
	setArea(a);
	calcPerimeter(a);
};

void Circle::calcPerimeter(double a)
{
	radius = sqrt(a / M_PI);
	setPerimeter((2*M_PI) * radius);
};

