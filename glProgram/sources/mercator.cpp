#include "framework.h"

const char *vertSource = R"(
    #version 330
    layout(location = 0) in vec2 position;
    out vec2 texCoord;

    void main() {
        texCoord = (position + vec2(1.0, 1.0)) / 2.0;
        gl_Position = vec4(position, 0.0, 1.0);
    }
)";

const char *fragSource = R"(
    #version 330
    uniform sampler2D mapTexture;
    uniform float time;
    uniform int drawMode;
    uniform vec3 color;

    in vec2 texCoord;
    out vec4 fragColor;

    const float PI = 3.141592653589793;
    const float tilt = 23.0 * PI / 180.0;

    bool isDaytime(vec2 latLon) {
        float declination = tilt;

        float hourAngle = (time * PI / 12.0) - PI + latLon.y;

        float sinAltitude = sin(declination) * sin(latLon.x) +
                          cos(declination) * cos(latLon.x) * cos(hourAngle);

        return sinAltitude > 0.0;
    }

    void main() {
        if (drawMode == 1) {
            fragColor = vec4(color, 1.0);
            return;
        }

        vec4 texColor = texture(mapTexture, texCoord);

        const float maxLat = 85.0 * PI / 180.0;
        const float maxMercY = log(tan(PI/4.0 + maxLat / 2.0));

        float longitude = (texCoord.x - 0.5) * 2.0 * PI;
        float mercY = (texCoord.y - 0.5) * 2.0 * maxMercY;
        float latitude = 2.0 * atan(exp(mercY)) - PI/2.0;

        vec2 latLon = vec2(latitude, longitude);

        if (!isDaytime(latLon)) texColor.rgb *= 0.5;

        fragColor = texColor;
    }
)";

const int winWidth = 600, winHeight = 600;

struct Station {
    vec2 position;
};

class MercatorMap : public glApp {
    GPUProgram *program;
    unsigned int textureID;
    std::vector<Station> *stations = {};
    Geometry<vec2> *quad = {};
    Geometry<vec2> *pathGeometry = {};
    Geometry<vec2> *pointGeometry = {};
    float currentTime = 0.0f;

    static std::vector<unsigned char> decodeTexture(const std::vector<unsigned char> &encoded) {
        std::vector<unsigned char> pixels;
        for (unsigned char byte: encoded) {
            int H = (byte >> 2) + 1;
            int I = byte & 0x03;
            for (int i = 0; i < H; ++i) {
                pixels.push_back(I);
            }
        }
        return pixels;
    }

    static vec3 indexToColor(int i) {
        switch (i) {
            case 0:
                return vec3(1.0f, 1.0f, 1.0f);
            case 1:
                return vec3(0.0f, 0.0f, 1.0f);
            case 2:
                return vec3(0.0f, 1.0f, 0.0f);
            default:
                return vec3(0.0f, 0.0f, 0.0f);
        }
    }

    static vec2 sphericalInterpolation(const vec2 &a, const vec2 &b, float t) {
        const float R = 1.0f;
        const float maxLatRad = 85.0f * M_PI / 180.0f;
        const float maxMercY = log(tan(M_PI / 4.0f + maxLatRad / 2.0f));

        float longitudeA = a.x * M_PI;
        float mercYA = a.y * maxMercY;
        float latitudeA = 2.0f * atan(exp(mercYA)) - M_PI / 2.0f;

        glm::vec3 p1 = glm::vec3(
                R * cos(latitudeA) * cos(longitudeA),
                R * cos(latitudeA) * sin(longitudeA),
                R * sin(latitudeA)
        );

        float longitudeB = b.x * M_PI;
        float mercYB = b.y * maxMercY;
        float latitudeB = 2.0f * atan(exp(mercYB)) - M_PI / 2.0f;

        glm::vec3 p2 = glm::vec3(
                R * cos(latitudeB) * cos(longitudeB),
                R * cos(latitudeB) * sin(longitudeB),
                R * sin(latitudeB)
        );

        float omega = acos(glm::dot(glm::normalize(p1), glm::normalize(p2)));
        float sinOmega = sin(omega);

        if (sinOmega == 0.0f) {
            return a;
        }

        glm::vec3 p = (sin((1.0f - t) * omega) / sinOmega) * p1 + (sin(t * omega) / sinOmega) * p2;

        float latResult = asin(p.z / R);
        float lonResult = atan2(p.y, p.x);

        lonResult = fmod(lonResult + 3.0f * M_PI, 2.0f * M_PI) - M_PI;

        float newMercY = log(tan(latResult / 2.0f + M_PI / 4.0f));
        return vec2(
                lonResult / M_PI,
                newMercY / maxMercY
        );
    }

    void calculatePath(const Station &a, const Station &b) {
        const int segments = 100;
        pathGeometry->Vtx().clear();

        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            vec2 point = sphericalInterpolation(a.position, b.position, t);
            pathGeometry->Vtx().push_back(point);
        }

        std::vector<vec2>& points = pathGeometry->Vtx();
        if (points.empty()) return;

        float prevLon = points[0].x * M_PI;

        for (size_t i = 1; i < points.size(); ++i) {
            float currentLon = points[i].x * M_PI;
            float delta = currentLon - prevLon;

            if (delta > M_PI) {
                currentLon -= 2.0f * M_PI;
            } else if (delta < -M_PI) {
                currentLon += 2.0f * M_PI;
            }

            points[i].x = currentLon / M_PI;
            prevLon = currentLon;
        }

        pathGeometry->updateGPU();
    }

public:
    MercatorMap() : glApp("Mercator projection") {}

    void onInitialization() override {
        stations = new std::vector<Station>;
        quad = new Geometry<vec2>;
        pathGeometry = new Geometry<vec2>;
        pointGeometry = new Geometry<vec2>;

        glPointSize(10);
        glLineWidth(3);

        program = new GPUProgram(vertSource, fragSource);
        program->Use();

        std::vector<unsigned char> encoded = { 252, 252, 252, 252, 252, 252, 252, 252, 252, 0, 9, 80, 1, 148, 13, 72, 13, 140, 25, 60, 21, 132, 41, 12, 1, 28,
                                               25, 128, 61, 0, 17, 4, 29, 124, 81, 8, 37, 116, 89, 0, 69, 16, 5, 48, 97, 0, 77, 0, 25, 8, 1, 8, 253, 253, 253, 253,
                                               101, 10, 237, 14, 237, 14, 241, 10, 141, 2, 93, 14, 121, 2, 5, 6, 93, 14, 49, 6, 57, 26, 89, 18, 41, 10, 57, 26,
                                               89, 18, 41, 14, 1, 2, 45, 26, 89, 26, 33, 18, 57, 14, 93, 26, 33, 18, 57, 10, 93, 18, 5, 2, 33, 18, 41, 2, 5, 2, 5, 6,
                                               89, 22, 29, 2, 1, 22, 37, 2, 1, 6, 1, 2, 97, 22, 29, 38, 45, 2, 97, 10, 1, 2, 37, 42, 17, 2, 13, 2, 5, 2, 89, 10, 49,
                                               46, 25, 10, 101, 2, 5, 6, 37, 50, 9, 30, 89, 10, 9, 2, 37, 50, 5, 38, 81, 26, 45, 22, 17, 54, 77, 30, 41, 22, 17, 58,
                                               1, 2, 61, 38, 65, 2, 9, 58, 69, 46, 37, 6, 1, 10, 9, 62, 65, 38, 5, 2, 33, 102, 57, 54, 33, 102, 57, 30, 1, 14, 33, 2,
                                               9, 86, 9, 2, 21, 6, 13, 26, 5, 6, 53, 94, 29, 26, 1, 22, 29, 0, 29, 98, 5, 14, 9, 46, 1, 2, 5, 6, 5, 2, 0, 13, 0, 13,
                                               118, 1, 2, 1, 42, 1, 4, 5, 6, 5, 2, 4, 33, 78, 1, 6, 1, 6, 1, 10, 5, 34, 1, 20, 2, 9, 2, 12, 25, 14, 5, 30, 1, 54, 13, 6,
                                               9, 2, 1, 32, 13, 8, 37, 2, 13, 2, 1, 70, 49, 28, 13, 16, 53, 2, 1, 46, 1, 2, 1, 2, 53, 28, 17, 16, 57, 14, 1, 18, 1, 14,
                                               1, 2, 57, 24, 13, 20, 57, 0, 2, 1, 2, 17, 0, 17, 2, 61, 0, 5, 16, 1, 28, 25, 0, 41, 2, 117, 56, 25, 0, 33, 2, 1, 2, 117,
                                               52, 201, 48, 77, 0, 121, 40, 1, 0, 205, 8, 1, 0, 1, 12, 213, 4, 13, 12, 253, 253, 253, 141 };
        std::vector<unsigned char> indices = decodeTexture(encoded);

        std::vector<vec3> colors;
        for (unsigned char i: indices)
            colors.push_back(indexToColor(i));

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_FLOAT, colors.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        program->create(vertSource, fragSource);
        quad->Vtx() = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        quad->updateGPU();
    }

    void onDisplay() override {
        glClearColor(0, 0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
#ifdef __APPLE__
        glViewport(0, 0, winHeight * 2, winHeight * 2);
#else
        glViewport(0, 0, winWidth, winHeight);
#endif
        program->Use();
        program->setUniform(0, "drawMode");
        program->setUniform(currentTime, "time");
        glBindTexture(GL_TEXTURE_2D, textureID);
        quad->Draw(program, GL_TRIANGLE_FAN, vec3(1.0f, 1.0f, 1.0f));

        program->setUniform(1, "drawMode");
        program->setUniform(vec3(1.0f, 1.0f, 0.0f), "color");
        glLineWidth(3.0f);
        for (size_t i = 1; i < stations->size(); ++i) {
            calculatePath(stations->at(i - 1), stations->at(i));
            pathGeometry->Draw(program, GL_LINE_STRIP, vec3(1.0f, 1.0f, 0.0f));
        }

        program->setUniform(vec3(1.0f, 0.0f, 0.0f), "color");
        pointGeometry->Draw(program, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
    }

    void onMousePressed(MouseButton button, int pX, int pY) override {
        if (button == MOUSE_LEFT) {
            vec2 pos(
                    2.0f * pX / winWidth - 1.0f,
                    1.0f - 2.0f * pY / winHeight
            );

            stations->push_back({pos});

            pointGeometry->Vtx().clear();
            for (const auto &station: *stations)
                pointGeometry->Vtx().push_back(station.position);
            pointGeometry->updateGPU();

            refreshScreen();
        }
    }

    void onKeyboard(int key) override {
        if (key == 'n') {
            currentTime = fmod(currentTime + 1.0f, 24.0f);
            refreshScreen();
        }
    }
};

MercatorMap app;