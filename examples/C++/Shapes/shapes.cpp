/*
*	Create Circle and Triangle shapes
*
*	Wayne J. Dupont
*
*   August 7, 2013
*/
#include <iostream>
#include <string>
#include "shape.h"
#include "circle.h"
#include "triangle.h"

using namespace std;

// Function to display the shape Name, Area and Perimeter
void displayShape(Shapes * s)
{
	// Get the name and display it
	cout << "Shape Name : ";
	cout << s->getName() + " \n\n";

	// Get and display the area
	cout << "Area = ";
	cout << s->getArea();
	cout << " \n\n";

	// Get and display the perimeter
	cout << "Perimeter = ";
	cout << s->getPerimeter();
	cout << "\n\n";
	
};

int main()
{
	char u;

	// Create Circle object and display parameters
	// Pass in the area
	Shapes * c = new Circle(5.0);
	displayShape(c);

	// Create Triangle objects passin in the sides

	// Create a Equilateral Triangle and display parameters
	Shapes * e = new Triangle(5.0, 5.0, 5.0);
	displayShape(e);

	// Create a Isosceles Triangle and diaplay parameters
	Shapes * i = new Triangle(5.0, 3.0, 5.0);
	displayShape(i);

	// Create a Scalene Triangle and display parameters
	Shapes * s = new Triangle(5.0, 8.0, 11.0);
	displayShape(s);

	// Wait for user input to end
	cin >> u;

	return 0;
};