/*
*	Define a Circle class
*
*	Wayne J. Dupont
*
*   August 7, 2013
*/
#ifndef CIRCLE_H
#define CIRCLE_H

class Circle: public Shapes
{
public:
	Circle(double a);

private:
	double radius;
	void calcPerimeter(double a);
};
#endif