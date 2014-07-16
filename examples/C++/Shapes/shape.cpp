/*
*	Shapes class implementation
*
*	Wayne J. Dupont
*
*   August 7, 2013
*/
#include <string>
#include "shape.h"

using namespace std;

Shapes::Shapes()
{
};

void Shapes::setName(string n)
{
	name = n;
};

string Shapes::getName()
{
	return name;
};

void Shapes::setArea(double a)
{
	area = a;
};

double Shapes::getArea()
{
	return area;
};

void Shapes::setPerimeter(double p)
{
	perimeter = p;
};

double Shapes::getPerimeter()
{
	return perimeter;
};