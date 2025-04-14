#include "framework.h"

const int winWidth = 600, winHeight = 600;

#define VEC3(v) vec3(v, v, v)

float radians(float deg) {
    return deg * (M_PI / 180.0f);
}

vec3 compDiv(const vec3& a, const vec3& b) {
    return vec3(a.x / b.x, a.y / b.y, a.z / b.z);
}

vec3 reflect(const vec3 &I, const vec3 &N) {
    return I - 2.0f * dot(I, N) * N;
}

vec3 refract(vec3 V, vec3 N, float ns) {
    float cosa = -dot(V, N);
    float disc = 1 - (1 - cosa*cosa)/ns/ns;
    if (disc < 0) return vec3(0, 0, 0);
    return V/ns + N * (cosa/ns - sqrt(disc));
}

mat4 perspective(float fov, float aspect, float near, float far) {
    float tanHalfFOV = tan(fov / 2.0f);
    mat4 result(0.0f);
    result[0][0] = 1.0f / (aspect * tanHalfFOV);
    result[1][1] = 1.0f / tanHalfFOV;
    result[2][2] = -(far + near) / (far - near);
    result[2][3] = -1.0f;
    result[3][2] = -(2.0f * far * near) / (far - near);
    return result;
}


mat4 inverseOrt(const  mat4& m) {
    mat4 inv(1.0f);

    float r00 = m[0][0], r01 = m[0][1], r02 = m[0][2];
    float r10 = m[1][0], r11 = m[1][1], r12 = m[1][2];
    float r20 = m[2][0], r21 = m[2][1], r22 = m[2][2];

    inv[0][0] = r00; inv[0][1] = r10; inv[0][2] = r20;
    inv[1][0] = r01; inv[1][1] = r11; inv[1][2] = r21;
    inv[2][0] = r02; inv[2][1] = r12; inv[2][2] = r22;

    float t0 = m[3][0], t1 = m[3][1], t2 = m[3][2];
    inv[3][0] = -(inv[0][0]*t0 + inv[1][0]*t1 + inv[2][0]*t2);
    inv[3][1] = -(inv[0][1]*t0 + inv[1][1]*t1 + inv[2][1]*t2);
    inv[3][2] = -(inv[0][2]*t0 + inv[1][2]*t1 + inv[2][2]*t2);
    inv[3][3] = 1.0f;

    return inv;
}


mat4 lookAt(const vec3 &eye, const vec3 &center, const vec3 &up) {
    vec3 f = normalize(center - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);

    mat4 result(1.0f);
    result[0][0] = s.x;
    result[1][0] = s.y;
    result[2][0] = s.z;
    result[0][1] = u.x;
    result[1][1] = u.y;
    result[2][1] = u.z;
    result[0][2] = -f.x;
    result[1][2] = -f.y;
    result[2][2] = -f.z;
    result[3][0] = -dot(s, eye);
    result[3][1] = -dot(u, eye);
    result[3][2] =  dot(f, eye);
    return result;
}

struct Material {
    vec3 ka, kd, ks;
    float shininess;
    bool reflective = false, refractive = false;
    vec3 ior = VEC3(1.0f);
    vec3 kappa = VEC3(0.0f);

    Material(vec3 _kd = vec3(0), vec3 _ks = vec3(0), float _shininess = 0)
            : kd(_kd), ks(_ks), shininess(_shininess) {
        ka = kd * float(M_PI);
    }
};

struct Hit {
    float t;
    vec3 position, normal;
    Material *material;
    bool entering;

    Hit() { t = -1; }

    Hit(float _t, vec3 pos, vec3 n, Material *mat, bool enter) :
            t(_t), position(pos), normal(n), material(mat), entering(enter) {}
};

struct Ray {
    vec3 start;
    vec3 dir;
    bool out;

    Ray(vec3 s, vec3 d, bool o = true) : start(s), dir(d), out(o) {}

};

vec3 Fresnel(const vec3 &n, const vec3 &kappa, float cosTheta) {
    vec3 numerator   = (n - VEC3(1.0f)) * (n - VEC3(1.0f)) + kappa * kappa;
    vec3 denominator = (n + VEC3(1.0f)) * (n + VEC3(1.0f)) + kappa * kappa;
    vec3 F0 = compDiv(numerator, denominator);
    return F0 + (VEC3(1.0f) - F0) * pow(1.0f - cosTheta, 5.0f);
}


class Intersectable {
public:
    Material *material;

    virtual Hit intersect(const Ray &ray) = 0;
};

class Plane : public Intersectable {
    vec3 point, normal;
public:
    Plane(vec3 p, vec3 n, Material *m) : point(p), normal(normalize(n)) { material = m; }

    Hit intersect(const Ray &ray) override {
        float d = dot(normal, ray.dir);
        if (fabs(d) < 1e-6f) return Hit();
        float t = dot(point - ray.start, normal) / d;
        if (t <= 0) return Hit();

        if (d > 0) return Hit();

        vec3 pos = ray.start + ray.dir * t;
        if (fabs(pos.x) > 10 || fabs(pos.z) > 10) return Hit();

        int xTile = (int(floor(pos.x + 10)) % 2);
        int zTile = (int(floor(pos.z + 10)) % 2);
        bool isWhite = (xTile + zTile) % 2 == 0;
        if (floor(pos.x) == 0 && floor(pos.z) == 0 && pos.y <= -0.999f && pos.y >= -1.001f) {
            isWhite = true;
        }

        material->kd = isWhite ? VEC3(0.3f) : vec3(0.0f, 0.1f, 0.3f);
        material->ka = material->kd * float(M_PI);

        return Hit(t, pos, normal, material, false);
    }
};

class Cylinder : public Intersectable {
    vec3 basePoint, axisDir;
    float radius, height;
public:
    Cylinder(vec3 b, vec3 d, float r, float h, Material *m)
            : basePoint(b), radius(r), height(h) {
        axisDir = normalize(d);
        material = m;
    }

    Hit intersect(const Ray &ray) override {
        Hit bestHit;

        vec3 oc = ray.start - basePoint;
        float a = dot(ray.dir, ray.dir) - pow(dot(ray.dir, axisDir), 2);
        float b = 2 * (dot(oc, ray.dir) - dot(oc, axisDir) * dot(ray.dir, axisDir));
        float c = dot(oc, oc) - pow(dot(oc, axisDir), 2) - radius * radius;
        float discr = b * b - 4 * a * c;

        if (discr >= 0) {
            float t1 = (-b - sqrt(discr)) / (2 * a);
            float t2 = (-b + sqrt(discr)) / (2 * a);
            std::vector<float> tCandidates = {t1, t2};

            for (float t: tCandidates) {
                if (t <= 0) continue;
                vec3 hitPoint = ray.start + ray.dir * t;
                float y = dot(hitPoint - basePoint, axisDir);
                if (y < 0 || y > height) continue;
                vec3 normal = (hitPoint - basePoint) - axisDir * y;
                normal = normalize(normal);
                bool entering = dot(normal, ray.dir) < 0;
                normal = entering ? normal : -normal;
                if (dot(normal, ray.dir) >= 0) continue;
                if (bestHit.t < 0 || t < bestHit.t) {
                    bestHit = Hit(t, hitPoint, normal, material, entering);
                }
            }
        }

        return bestHit;
    }
};

class Cone : public Intersectable {
    vec3 apex, axisDir;
    float angle, height;
public:
    Cone(vec3 a, vec3 d, float ang, float h, Material *m)
            : apex(a), axisDir(normalize(d)), angle(ang), height(h) { material = m; }

    Hit intersect(const Ray &ray) override {
        vec3 co = ray.start - apex;
        float cosTheta = cos(angle);
        float a = pow(dot(ray.dir, axisDir), 2) - pow(cosTheta, 2) * dot(ray.dir, ray.dir);
        float b = 2 * (dot(ray.dir, axisDir) * dot(co, axisDir) - pow(cosTheta, 2) * dot(co, ray.dir));
        float c = pow(dot(co, axisDir), 2) - pow(cosTheta, 2) * dot(co, co);
        float discr = b * b - 4 * a * c;

        if (discr < 0) return Hit();
        float sqrtDiscr = sqrt(discr);

        std::vector<float> tCandidates;
        float t1 = (-b - sqrtDiscr) / (2 * a);
        float t2 = (-b + sqrtDiscr) / (2 * a);

        auto checkT = [&](float t) {
            if (t <= 0) return false;
            vec3 hitPoint = ray.start + ray.dir * t;
            float y = dot(hitPoint - apex, axisDir);
            return (y >= 0 && y <= height);
        };

        if (checkT(t1)) tCandidates.push_back(t1);
        if (checkT(t2)) tCandidates.push_back(t2);

        if (tCandidates.empty()) return Hit();
        float t = tCandidates[0];
        for (size_t i = 1; i < tCandidates.size(); ++i) {
            if (tCandidates[i] < t)
                t = tCandidates[i];
        }

        vec3 hitPoint = ray.start + ray.dir * t;

        vec3 toApex = hitPoint - apex;
        vec3 gradient = dot(toApex, axisDir) * axisDir - toApex * (cosTheta * cosTheta);
        vec3 normal = normalize(gradient);

        if (dot(normal, ray.dir) > 0) normal = -normal;

        return Hit(t, hitPoint, normal, material, false);
    }
};

class Scene {
public:
    std::vector<Intersectable *> objects;
    vec3 cameraPos, La = VEC3(0.4f);
    mat4 viewMatrix, projMatrix;
    vec3 Le = VEC3(2.0f);

    void build() {
        cameraPos = vec3(0, 1, 4);
        viewMatrix = lookAt(cameraPos, vec3(0), vec3(0, 1, 0));
        projMatrix = perspective(radians(45.0f), winWidth/(float)winHeight, 0.1f, 100.0f);
        objects.clear();

        Material *floorMat = new Material();
        objects.push_back(new Plane(vec3(0, -1, 0), vec3(0, 1, 0), floorMat));

        Material *gold = new Material(VEC3(0.0f), VEC3(0.9f), 200.0f);
        gold->reflective = true;
        gold->ior = vec3(0.17f, 0.35f, 1.5f);
        gold->kappa = vec3(3.1f, 2.7f, 1.9f);
        objects.push_back(new Cylinder(vec3(1, -1, 0), vec3(0.1f, 1, 0), 0.3f, 2.0f, gold));

        Material *water = new Material(vec3(0.0f), vec3(0.0f), 100.0f);
        water->reflective = true;
        water->refractive = true;
        water->ior = VEC3(1.3f);
        water->kappa = VEC3(0.0f);
        objects.push_back(new Cylinder(vec3(0, -1, -0.8f), vec3(-0.2f, 1, -0.1f), 0.3f, 2.0f, water));

        Material *yellow = new Material(vec3(0.3f, 0.2f, 0.1f), VEC3(2.0f), 50.0f);
        objects.push_back(new Cylinder(vec3(-1, -1, 0), vec3(0, 1, 0.1f), 0.3f, 2.0f, yellow));

        Material *cyan = new Material(vec3(0.1f, 0.2f, 0.3f), VEC3(2.0f), 100.0f);
        objects.push_back(new Cone(vec3(0, 1, 0), vec3(-0.1f, -1, -0.05f), 0.2f, 2.0f, cyan));

        Material *magenta = new Material(vec3(0.3f, 0.0f, 0.2f), VEC3(2.0f), 20.0f);
        objects.push_back(new Cone(vec3(0, 1, 0.8f), vec3(0.2f, -1, 0), 0.2f, 2.0f, magenta));
    }

    vec3 trace(const Ray &ray, int depth = 0) {

        if (depth > 8) return La;

        Hit best;
        for (auto obj: objects) {
            Hit h = obj->intersect(ray);
            if (h.t > 0 && (best.t < 0 || h.t < best.t)) best = h;
        }
        if (best.t < 0) return La;

        vec3 N = best.normal;
        if (dot(N, ray.dir) > 0) N = -N;

        vec3 L = normalize(vec3(1, 1, 1));
        vec3 shadowRayStart = best.position + N * 1e-4f;
        Ray shadowRay(shadowRayStart, L);
        bool inShadow = false;
        for (auto obj : objects) {
            Hit shadowHit = obj->intersect(shadowRay);
            if (shadowHit.t > 0) {
                inShadow = true;
                break;
            }
        }

        vec3 H = normalize(-ray.dir + L);
        float cosTheta = dot(N, L);
        float cosDelta = dot(N, H);
        vec3 outRad = best.material->ka * La;

        if (!inShadow && cosTheta > 0) {
            outRad += Le * (best.material->kd * cosTheta +
                            best.material->ks * powf(fmaxf(0.0f, cosDelta), best.material->shininess));
        }

        if (best.material->reflective) {
            vec3 reflectedDir = reflect(ray.dir, N);
            Ray reflectedRay(best.position + N * 1e-4f, reflectedDir);
            vec3 F = Fresnel(best.material->ior, best.material->kappa,
                             fmax(dot(-ray.dir, N), 0.0f));
            outRad += trace(reflectedRay, depth + 1) * F;
        }

        if (best.material->refractive) {
            float ns = best.entering ? best.material->ior.x : (1.0f / best.material->ior.x);
            vec3 refractionDir = refract(ray.dir, N, ns);
            if (length(refractionDir) > 0) {
                Ray refractRay(best.position - N * 1e-4f, normalize(refractionDir));
                vec3 F = Fresnel(best.material->ior, best.material->kappa,fmax(dot(-ray.dir, N), 0.0f));
                outRad += trace(refractRay, depth + 1) * (VEC3(1.0f) - F);
            }
        }
        return outRad;
    }

    void render(std::vector<vec3> &framebuffer) {
        framebuffer.resize(winWidth * winHeight);
        float fov = radians(45.0f);
        for (int y = 0; y < winHeight; y++) {
            for (int x = 0; x < winWidth; x++) {
                int invertedY = winHeight - 1 - y;
                float u = (2.0f * x / winWidth - 1.0f) * tan(fov / 2);
                float v = (1 - 2.0f * invertedY / winHeight) * tan(fov / 2);
                vec3 rayDir = normalize(vec3(u, v, -1));
                vec4 temp = inverseOrt(viewMatrix) * vec4(rayDir.x, rayDir.y, rayDir.z, 0.0f);
                rayDir = vec3(temp.x, temp.y, temp.z);
                framebuffer[y * winWidth + x] = trace(Ray(cameraPos, rayDir));
            }
        }
    }
} scene;

const char *vertexSource = R"(
#version 330
layout(location = 0) in vec2 cVertexPosition;
out vec2 texcoord;
void main() {
    texcoord = (cVertexPosition + vec2(1, 1)) / 2;
    gl_Position = vec4(cVertexPosition.x, cVertexPosition.y, 0, 1);
})";

const char *fragmentSource = R"(
#version 330
uniform sampler2D textureUnit;
in vec2 texcoord;
out vec4 fragmentColor;
void main() {
    fragmentColor = texture(textureUnit, texcoord);
})";

class FullScreenTexturedQuad : public Geometry<vec2> {
    GLuint textureId = 0;
public:
    FullScreenTexturedQuad() {
        vtx = { vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, 1) };
        updateGPU();
    }

    void updateTexture(int w, int h, std::vector<vec3>& image) {
        if (textureId == 0)
            glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_FLOAT, &image[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    void Draw() {
        Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    ~FullScreenTexturedQuad() {
        if (textureId > 0)
            glDeleteTextures(1, &textureId);
    }
};


class RaytraceApp : public glApp {
    FullScreenTexturedQuad *quad;
    GPUProgram shader;
public:
    RaytraceApp() : glApp("Ray Tracer") {}

    void onInitialization() {
#ifdef __APPLE__
        glViewport(0, 0, winHeight * 2, winHeight * 2);
#else
        glViewport(0, 0, winWidth, winHeight);
#endif
        scene.build();
        quad = new FullScreenTexturedQuad;
        shader.create(vertexSource, fragmentSource);
    }

    void onKeyboard(int key) override {
        if (key == 'a') {
            static float angle = 0;
            angle += M_PI / 4;
            scene.cameraPos.x = 4 * sin(angle);
            scene.cameraPos.z = 4 * cos(angle);
            scene.viewMatrix = lookAt(scene.cameraPos, vec3(0), vec3(0, 1, 0));
            refreshScreen();
        }
    }

    void onDisplay() override {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        std::vector<vec3> image;
        scene.render(image);
        quad->updateTexture(winWidth, winHeight, image);
        shader.Use();
        shader.setUniform(0, "textureUnit");
        quad->Draw();
    }
} app;