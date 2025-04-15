# GrafikaProjekt

Course project for the 4th-semester Computer Graphics class at BME-VIK.

## Contents

- Points and lines interaction demo
- Rollercoaster spline and gondola animation
- Mercator projection visualization
- Cylinder ray intersection demo
- macOS compatibility fixes (viewport handling for Retina displays)

## Build

Choose demo by name with the DEMO option.

Available demos:

- mercator
- pointsLines
- rollercoaster
- hengerekRay
- greenTriangle

## Run

Run with one command:

cmake -S . -B build -DDEMO=<DEMO> && cmake --build build --target run

## Controls

- greenTriangle
	- No interaction.

- mercator
	- Left click: add a station point.
	- `n`: advance time (day/night shading).

- pointsLines
	- `p`: point mode, left click adds a point.
	- `l`: line mode, select 2 existing points with left click to create a line.
	- `m`: move mode, left click near a line then drag with mouse to move it.
	- `i`: intersection mode, click two lines to add their intersection point.

- rollercoaster
	- Left click: add control points to the spline.
	- `Space`: start gondola animation (requires at least 2 control points).

- hengerekRay
	- `a`: rotate camera around the scene.

