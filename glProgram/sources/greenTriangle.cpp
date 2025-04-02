//=============================================================================================
// Z�ld h�romsz�g: A framework.h oszt�lyait felhaszn�l� megold�s
//=============================================================================================
#include "framework.h"

// cs�cspont �rnyal�
const char * vertSource = R"(
	#version 330
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1); 	// bemenet m�r normaliz�lt eszk�zkoordin�t�kban
	}
)";

// pixel �rnyal�
const char * fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans sz�n
	out vec4 fragmentColor;		// pixel sz�n

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;
const int retinaWidth = winWidth*2, retinaHeight = winHeight*2;

class GreenTriangleApp : public glApp {
    Geometry<vec2>* triangle;  // geometria
    GPUProgram* gpuProgram;	   // cs�cspont �s pixel �rnyal�k
public:
    GreenTriangleApp() : glApp("Green triangle") { }

    // Inicializ�ci�,
    void onInitialization() {
        triangle = new Geometry<vec2>;
        triangle->Vtx() = { vec2(-0.8f, -0.8f), vec2(-0.6f, 1.0f), vec2(0.8f, -0.2f) };
        triangle->updateGPU();
        gpuProgram = new GPUProgram(vertSource, fragSource);
    }

    // Ablak �jrarajzol�s
    void onDisplay() {
        glClearColor(0, 0, 0, 0);     // h�tt�r sz�n
        glClear(GL_COLOR_BUFFER_BIT); // rasztert�r t�rl�s
        glViewport(0, 0, retinaWidth, retinaHeight);
        triangle->Draw(gpuProgram, GL_TRIANGLES, vec3(0.3f, 0.3f, 0.7f));
    }
};

GreenTriangleApp app;
