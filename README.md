# Final Project for Graphics
## Rubin Peci Period 5

## Features
- light
	- Add a light to the symbol table
	- When calculating diffuse and specular: loop through all the lights.
- add NEW shapes
	- cone: takes in 5 doubles (cx, cy, cz, r, h) to make a cone 
		- cx, cy, cz represent the center of the base of the cone
		- r represents the radius of the base of the cone
		- h represents the height of the cone
	- cylinder: takes in 5 integers (cx, cy, cz, r, h) to make a cylinder
		- cx, cy, cz represent the center of the base of the cylinder
		- r represents the radius of the base of the cylinder
		- h represents the height of the cylinder 
	- prism: takes in 10 integers (x0, y0, z0, x1, y1, z1, x2, y2, z2, h) to make a triangular prism
		- x0, y0, z0, x1, y1, z1, x2, y2, z2 represent the points of the base of the triangular prism
		- h represents the height
