/*
*	Triangle class implementation
*
*	Wayne J. Dupont
*
*   August 7, 2013
*/
#include "triangle.h"

Triangle::Triangle(double s1, double s2, double s3): Shapes()
{
	// Save sides
	side1 = s1;
	side2 = s2;
	side3 = s3;

		// Equilateral 
		if(side1==side2 && side2==side3)
		{
			setName("Equilateral Triangle");
			setArea(calcArea());
			setPerimeter(calcPerimeter());
		}
		else if(side1==side2 || side1==side3 || side2==side3)   // Isosceles
		{
			setName("Isosceles Triangle");
			setArea(calcArea());
			setPerimeter(calcPerimeter());
		}
		else // Scalene
		{
			setName("Scalene Triangle");
			setArea(calcArea());
			setPerimeter(calcPerimeter());
		};
	};

	// Calculete the area
	double Triangle::calcArea()
	{
		double S = (side1+side2+side3)/2.0;						// S is the semiperimeter of the triangle
		double D = S*(S-side1)*(S-side2)*(S-side3);				// D is the square of the area of the triangle

		return sqrt(D);
	};

	// Calculate the perimeter
	double Triangle::calcPerimeter()
	{
		double S = (side1+side2+side3)/2.0;						// S is the semiperimeter of the triangle

		return (2.0 * S);
	};


