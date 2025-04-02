#include "framework.h"

const char *vertSource = R"(
	#version 330
    precision highp float;

	layout(location = 0) in vec2 cP;

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1);
}
)";

const char *fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;
	out vec4 fragmentColor;

	void main() {
		fragmentColor = vec4(color, 1);
	}
)";


enum State {
    POINT,
    LINE,
    MOVE,
    INTERSECT
};
const int winWidth = 600, winHeight = 600;

class pointsLines : public glApp {
    State state = POINT;
    Geometry<vec2> *points{};
    std::vector<vec2> selectedPoints;
    Geometry<vec2> *lines{};
    vec2 selectedLine[2]{};
    bool isMoving = false;
    size_t selectedLineIndex = -1;
    std::vector<vec2> selectedLinesForIntersection;
    GPUProgram *gpuProgram{};
public:
    pointsLines() : glApp("Points and lines") {}

    void onInitialization() override {
        glPointSize(10);
        glLineWidth(3);
        points = new Geometry<vec2>;
        lines = new Geometry<vec2>;
        gpuProgram = new GPUProgram(vertSource, fragSource);
    }

    void onDisplay() override {
        glClearColor(0.2, 0.2, 0.2, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
#ifdef __APPLE__
        glViewport(0, 0, winHeight * 2, winHeight * 2); // For retina display
#else
        glViewport(0, 0, winWidth, winHeight); // For JPorta
#endif
        lines->Bind();
        lines->Draw(gpuProgram, GL_LINES, vec3(0.0f, 1.0f, 1.0f));
        points->Bind();
        points->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
    }

    void AddPoint(const vec2 &position) {
        printf("Point %1.1f, %1.1f added\n", position.x, position.y);
        points->Vtx().push_back(position);
        points->updateGPU();
        refreshScreen();
    }

    bool FindLine(const vec2 &clickPos, vec2 *p1 = nullptr, vec2 *p2 = nullptr, bool select = false) {
        for (size_t i = 0; i < lines->Vtx().size(); i += 2) {
            vec2 lp1 = lines->Vtx()[i];
            vec2 lp2 = lines->Vtx()[i + 1];

            float d = fabs((lp2.y - lp1.y) * clickPos.x - (lp2.x - lp1.x) * clickPos.y + lp2.x * lp1.y - lp2.y * lp1.x)
                      / length(lp2 - lp1);

            if (d < 0.05f) {
                if (p1 && p2) {
                    *p1 = lp1;
                    *p2 = lp2;
                }
                if (select) {
                    selectedLine[0] = lp1;
                    selectedLine[1] = lp2;
                    selectedLineIndex = i;
                    isMoving = true;
                }
                return true;
            }
        }
        return false;
    }

    static bool ComputeIntersection(vec2 p1, vec2 p2, vec2 p3, vec2 p4, vec2 &intersection) {
        float A1 = p1.y - p2.y;
        float B1 = p2.x - p1.x;
        float C1 = p1.x * p2.y - p2.x * p1.y;

        float A2 = p3.y - p4.y;
        float B2 = p4.x - p3.x;
        float C2 = p3.x * p4.y - p4.x * p3.y;

        float det = A1 * B2 - A2 * B1;

        if (fabs(det) > 0.05f) {
            intersection.x = (B1 * C2 - B2 * C1) / det;
            intersection.y = (A2 * C1 - A1 * C2) / det;
            return (intersection.x >= -1 && intersection.x <= 1 && intersection.y >= -1 && intersection.y <= 1);
        }
        return false;
    }

    static vec2 PixelToNDC(int pX, int pY) {
        return {2.0f * (float) pX / winWidth - 1, 1.0f - 2.0f * (float) pY / winHeight};
    }

    void onMousePressed(MouseButton but, int pX, int pY) override {
        if (but == MOUSE_LEFT) {
            vec2 clickPos = PixelToNDC(pX, pY);
            switch (state) {
                case POINT:
                    AddPoint(clickPos);
                    break;
                case LINE:
                    for (vec2 &p: points->Vtx()) {
                        if (length(p - clickPos) < 0.05f) {
                            selectedPoints.push_back(p);
                            break;
                        }
                    }

                    if (selectedPoints.size() == 2) {
                        vec2 p1 = selectedPoints[0];
                        vec2 p2 = selectedPoints[1];

                        if (length(p2 - p1) < 0.05f) {
                            printf("Distance between the two selected points is under the threshold.\n");
                            selectedPoints.clear();
                            return;
                        }

                        vec2 d = p2 - p1;

                        float t1, t2;
                        vec2 pMin, pMax;

                        if (fabs(d.x) > fabs(d.y)) {
                            t1 = (-1 - p1.x) / d.x;
                            t2 = (1 - p1.x) / d.x;
                        } else {
                            t1 = (-1 - p1.y) / d.y;
                            t2 = (1 - p1.y) / d.y;
                        }

                        pMin = p1 + t1 * d;
                        pMax = p1 + t2 * d;

                        lines->Vtx().push_back(pMin);
                        lines->Vtx().push_back(pMax);
                        lines->updateGPU();
                        refreshScreen();

                        float A = p1.y - p2.y;
                        float B = p2.x - p1.x;
                        float C = p1.x * p2.y - p2.x * p1.y;
                        printf("  Implicit: %1.1f x + %1.1f y + %1.1f = 0\n", A, B, C);

                        printf("Parametric: r(t) = (%1.1f, %1.1f) + (%1.1f, %1.1f)t\n",
                               p1.x, p1.y, d.x, d.y);

                        selectedPoints.clear();
                    }
                    break;
                case MOVE:
                    FindLine(clickPos, nullptr, nullptr, true);
                    break;
                case INTERSECT:
                    vec2 p1, p2, intersection;
                    if (FindLine(clickPos, &p1, &p2, false)) {
                        selectedLinesForIntersection.push_back(p1);
                        selectedLinesForIntersection.push_back(p2);
                    }

                    if (selectedLinesForIntersection.size() == 4) {
                        if (ComputeIntersection(selectedLinesForIntersection[0], selectedLinesForIntersection[1],
                                                selectedLinesForIntersection[2], selectedLinesForIntersection[3],
                                                intersection)) {
                            AddPoint(intersection);
                        }
                        selectedLinesForIntersection.clear();
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void onKeyboard(int key) override {
        switch (key) {
            case 'p':
                state = POINT;
                break;
            case 'l':
                state = LINE;
                printf("Define lines\n");
                break;
            case 'm':
                state = MOVE;
                printf("Move\n");
                break;
            case 'i':
                state = INTERSECT;
                printf("Intersect\n");
                break;
            default:
                break;
        }
    }

    static std::pair<vec2, vec2> ClipToViewport(vec2 p1, vec2 p2) {
        vec2 d = normalize(p2 - p1);
        float t1 = (-1 - p1.x) / d.x;
        float t2 = (1 - p1.x) / d.x;
        if (fabs(d.x) <= fabs(d.y)) {
            t1 = (-1 - p1.y) / d.y;
            t2 = (1 - p1.y) / d.y;
        }
        return {p1 + t1 * d, p1 + t2 * d};
    }

    void onMouseMotion(int pX, int pY) override {
        vec2 newPos = PixelToNDC(pX, pY);
        if (!isMoving || selectedLineIndex == -1) return;

        vec2 midPoint = (selectedLine[0] + selectedLine[1]) * 0.5f;
        vec2 delta = newPos - midPoint;

        selectedLine[0] += delta;
        selectedLine[1] += delta;

        std::pair<vec2, vec2> clipped = ClipToViewport(selectedLine[0], selectedLine[1]);
        lines->Vtx()[selectedLineIndex] = clipped.first;
        lines->Vtx()[selectedLineIndex + 1] = clipped.second;

        lines->updateGPU();
        refreshScreen();
    }

    void onMouseReleased(MouseButton but, int pX, int pY) override {
        if (but == MOUSE_LEFT) {
            if (isMoving) {
                isMoving = false;
                selectedLineIndex = -1;
            }
        }
    }
};

pointsLines app;