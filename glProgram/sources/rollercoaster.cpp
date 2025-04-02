#include "framework.h"

const char *vertSource = R"(
    #version 330
    precision highp float;

    layout(location = 0) in vec2 cP;
    uniform mat4 MVP;

    void main() {
        gl_Position = MVP * vec4(cP.x, cP.y, 0, 1);
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

const int winWidth = 600, winHeight = 600;

class Camera2D {
    vec2 wCenter = vec2(0, 0);
    vec2 wSize = vec2(20, 20);
public:
    mat4 V() const { return translate(vec3(-wCenter.x, -wCenter.y, 0)); }

    mat4 P() const { return scale(vec3(2 / wSize.x, 2 / wSize.y, 1)); }

    mat4 Vinv() const { return translate(vec3(wCenter.x, wCenter.y, 0)); }

    mat4 Pinv() const { return scale(vec3(wSize.x / 2, wSize.y / 2, 1)); }

};

class Spline {
    std::vector<vec2> controlPoints;
    Geometry<vec2> splineGeometry;
    std::vector<float> arcLengths;
    Geometry<vec2> controlPointGeometry;

public:
    void AddControlPoint(vec2 point) {
        controlPoints.push_back(point);
        UpdateSpline();
        CalculateArcLengths();
    }

    Spline() {
        controlPointGeometry.Vtx() = {
                vec2(-0.05f, -0.05f), vec2(0.05f, -0.05f),
                vec2(0.05f, 0.05f), vec2(-0.05f, 0.05f)
        };
        controlPointGeometry.updateGPU();
    }

    void DrawControlPoints(GPUProgram *gpuProgram, Camera2D *camera) {
        mat4 MVP = camera->P() * camera->V();
        gpuProgram->setUniform(MVP, "MVP");
        gpuProgram->setUniform(vec3(1.0f, 0.0f, 0.0f), "color");
        for (auto &cp: controlPoints) {
            mat4 M = translate(vec3(cp.x, cp.y, 0));
            gpuProgram->setUniform(MVP * M, "MVP");
            controlPointGeometry.Draw(gpuProgram, GL_POINTS, vec3(1, 0, 0));
        }
    }

    void UpdateSpline() {
        splineGeometry.Vtx().clear();
        if (controlPoints.size() >= 2) {
            for (int i = 0; i < controlPoints.size() - 1; i++) {
                vec2 p0 = (i == 0) ? controlPoints[i] : controlPoints[i - 1];
                vec2 p1 = controlPoints[i];
                vec2 p2 = controlPoints[i + 1];
                vec2 p3 = (i + 2 < controlPoints.size()) ? controlPoints[i + 2] : controlPoints[i + 1];

                for (float t = 0; t <= 1; t += 0.02f) {
                    vec2 point = CatmullRom(p0, p1, p2, p3, t);
                    splineGeometry.Vtx().push_back(point);
                }
            }
            splineGeometry.updateGPU();
        }
    }

    static vec2 CatmullRom(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }

    void CalculateArcLengths() {
        arcLengths.clear();
        if (controlPoints.size() < 2) return;

        arcLengths.push_back(0.0f);
        float totalLength = 0.0f;

        for (int i = 0; i < controlPoints.size() - 1; i++) {
            vec2 p0 = (i == 0) ? controlPoints[i] : controlPoints[i - 1];
            vec2 p1 = controlPoints[i];
            vec2 p2 = controlPoints[i + 1];
            vec2 p3 = (i + 2 < controlPoints.size()) ? controlPoints[i + 2] : controlPoints[i + 1];

            vec2 prevPoint = CatmullRom(p0, p1, p2, p3, 0.0f);
            for (float t = 0.02f; t <= 1.0f; t += 0.02f) {
                vec2 currentPoint = CatmullRom(p0, p1, p2, p3, t);
                totalLength += length(currentPoint - prevPoint);
                arcLengths.push_back(totalLength);
                prevPoint = currentPoint;
            }
        }
    }

    float GetYAtParam(float param) {
        int segment = static_cast<int>(param);
        float t = param - segment;
        if (segment < 0 || segment >= controlPoints.size() - 1) return 0.0f;

        vec2 p0 = (segment == 0) ? controlPoints[segment] : controlPoints[segment - 1];
        vec2 p1 = controlPoints[segment];
        vec2 p2 = controlPoints[segment + 1];
        vec2 p3 = (segment + 2 < controlPoints.size()) ? controlPoints[segment + 2] : controlPoints[segment + 1];

        vec2 point = CatmullRom(p0, p1, p2, p3, t);
        return point.y;
    }

    float GetMaxParam() {
        return controlPoints.size() - 1;
    }

    vec2 GetPositionAtParam(float param) {
        if (controlPoints.size() < 2) return vec2(0);
        param = fmax(0, fmin(param, controlPoints.size() - 1));

        int segment = static_cast<int>(param);
        segment = std::max(0, std::min(segment, static_cast<int>(controlPoints.size() - 2)));

        float t = param - segment;
        vec2 p0 = (segment == 0) ? controlPoints[0] : controlPoints[segment - 1];
        vec2 p1 = controlPoints[segment];
        vec2 p2 = controlPoints[segment + 1];
        vec2 p3 = (segment + 2 < controlPoints.size()) ? controlPoints[segment + 2] : p2;

        return CatmullRom(p0, p1, p2, p3, t);
    }

    vec2 GetTangentAtParam(float param) {
        if (controlPoints.size() < 2) return vec2(1.0f, 0.0f);
        int segment = static_cast<int>(param);
        segment = std::max(0, std::min(segment, static_cast<int>(controlPoints.size() - 2)));
        float t = param - segment;
        if (segment < 0 || segment >= controlPoints.size() - 1) return vec2(1.0f, 0.0f);

        vec2 p0 = (segment == 0) ? controlPoints[segment] : controlPoints[segment - 1];
        vec2 p1 = controlPoints[segment];
        vec2 p2 = controlPoints[segment + 1];
        vec2 p3 = (segment + 2 < controlPoints.size()) ? controlPoints[segment + 2] : controlPoints[segment + 1];

        float t2 = t * t;
        vec2 rawTangent = 0.5f * ((-p0 + p2) + (4.0f * p0 - 10.0f * p1 + 8.0f * p2 - 2.0f * p3) * t +
                                  (-3.0f * p0 + 9.0f * p1 - 9.0f * p2 + 3.0f * p3) * t2);
        return normalize(rawTangent);
    }

    void Draw(GPUProgram *gpuProgram, Camera2D *camera) {
        mat4 MVP = camera->P() * camera->V();
        gpuProgram->setUniform(MVP, "MVP");
        splineGeometry.Draw(gpuProgram, GL_LINE_STRIP, vec3(1.0f, 1.0f, 0.0f));
    }

    int GetControlPointCount() const { return controlPoints.size(); }

};

class Gondola {
    Spline *spline;
    float currentParam = 0.0f;
    float speed = 0.0f;
    float angle = 0.0f;
    bool isMoving = false;
    float radius = 1.0f;
    vec2 position;

    Geometry<vec2> fillGeometry;
    Geometry<vec2> outlineGeometry;
    Geometry<vec2> spokesGeometry;
    bool isVisible = false;

public:
    Gondola(Spline *spline) : spline(spline) {
        InitializeGeometry();
    }

    bool IsMoving() const { return isMoving; }

    void InitializeGeometry() {
        const int resolution = 360;
        const float outlineRadius = radius * 1.05f;

        fillGeometry.Vtx().clear();
        for (int i = 0; i < resolution; i++) {
            float theta = 2 * M_PI * i / resolution;
            fillGeometry.Vtx().push_back(vec2(radius * cos(theta), radius * sin(theta)));
        }
        fillGeometry.updateGPU();

        outlineGeometry.Vtx().clear();
        for (int i = 0; i < resolution; i++) {
            float theta = 2 * M_PI * i / resolution;
            outlineGeometry.Vtx().push_back(vec2(outlineRadius * cos(theta), outlineRadius * sin(theta)));
        }
        outlineGeometry.updateGPU();

        spokesGeometry.Vtx().clear();
        for (int i = 0; i < 360; i += 30) {
            float theta = i * M_PI / 180.0f;
            spokesGeometry.Vtx().push_back(vec2(0.0f, 0.0f));
            spokesGeometry.Vtx().push_back(vec2(radius * cos(theta), radius * sin(theta)));
        }
        spokesGeometry.updateGPU();
    }

    void Start() {
        currentParam = 0.01f;
        speed = 0.0f;
        angle = 0.0f;
        isMoving = true;
        isVisible = true;

        vec2 startPos = spline->GetPositionAtParam(currentParam);
        vec2 tangent = normalize(spline->GetTangentAtParam(currentParam));
        vec2 normal(-tangent.y, tangent.x);
        position = startPos + normal * radius;
    }

    void Animate(float dt) {
        if (!isMoving) return;

        float g = 10.0f;
        float lambda = 75.0f;
        float y0 = spline->GetYAtParam(0.0f);
        float yCurrent = spline->GetYAtParam(currentParam);
        speed = sqrt(2 * g * (y0 - yCurrent) / (1 + lambda));

        vec2 tangent = spline->GetTangentAtParam(currentParam);
        vec2 normal = vec2(-tangent.y, tangent.x);

        float tangentLength = length(tangent);
        if (tangentLength < 1e-6f) return;

        float ds = speed * dt;
        float dParam = ds / tangentLength;
        currentParam += dParam;

        angle -= dt * speed / radius;

        vec2 newPos = spline->GetPositionAtParam(currentParam);
        vec2 newTangent = spline->GetTangentAtParam(currentParam);
        vec2 newNormal = normalize(vec2(-newTangent.y, newTangent.x));
        position = newPos + newNormal * radius;

        if (currentParam >= spline->GetMaxParam()) {
            isMoving = false;
        }
    }

    mat4 GetModelMatrix() const {
        return translate(vec3(position.x, position.y, 0)) * rotate(angle, vec3(0, 0, 1));
    }

    void Draw(GPUProgram *gpuProgram, Camera2D *camera) {
        if (!isVisible) return;

        mat4 MVP = camera->P() * camera->V() * GetModelMatrix();
        gpuProgram->setUniform(MVP, "MVP");

        fillGeometry.Draw(gpuProgram, GL_TRIANGLE_FAN, vec3(0, 0, 1));

        outlineGeometry.Draw(gpuProgram, GL_LINE_LOOP, vec3(1, 1, 1));

        spokesGeometry.Draw(gpuProgram, GL_LINES, vec3(1, 1, 1));
    }
};

class RollerCoasterApp : public glApp {
    Spline *spline;
    Gondola *gondola;
    Camera2D *camera;
    GPUProgram *gpuProgram;
public:
    RollerCoasterApp() : glApp("Rollercoaster") {}

    void onInitialization() override {
        glPointSize(10);
        glLineWidth(3);
        spline = new Spline;
        gondola = new Gondola(spline);
        camera = new Camera2D;

        gpuProgram = new GPUProgram(vertSource, fragSource);
    }

    void onDisplay() override {
        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
#ifdef __APPLE__
        glViewport(0, 0, winHeight * 2, winHeight * 2);
#else
        glViewport(0, 0, winWidth, winHeight);
#endif
        spline->Draw(gpuProgram, camera);
        gondola->Draw(gpuProgram, camera);
        spline->DrawControlPoints(gpuProgram, camera);
    }

    void onKeyboard(int key) override {
        if (key == ' ' && !gondola->IsMoving()) {
            if (spline->GetControlPointCount() >= 2) {
                gondola->Start();
                refreshScreen();
            }
        }
    }

    void onMousePressed(MouseButton but, int pX, int pY) override {
        if (but == MOUSE_LEFT) {
            float ndcX = 2.0f * (float) pX / winWidth - 1.0f;
            float ndcY = 1.0f - 2.0f * (float) pY / winHeight;
            vec4 worldPos = camera->Vinv() * camera->Pinv() * vec4(ndcX, ndcY, 0.0f, 1.0f);
            spline->AddControlPoint(vec2(worldPos.x, worldPos.y));
            refreshScreen();
        }
    }

    void onTimeElapsed(float tstart, float tend) override {
        const float dt = 0.01;
        for (float t = tstart; t < tend; t += dt) {
            float Dt = fmin(dt, tend - t);
            gondola->Animate(Dt);
        }
        refreshScreen();
    }
};

RollerCoasterApp app;