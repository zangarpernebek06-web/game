// Star Catcher - OpenGL 3.3 core (GLEW + GLFW, GLSL shaders, VAO/VBO)
// Build: g++ main.cpp -o game -lglfw -lGLEW -lGL
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

static const int W = 800;
static const int H = 600;

// ---------------------------------------------------------------- types
struct Col { float r, g, b, a; };
static Col C(int r, int g, int b, int a = 255) { return { r / 255.f, g / 255.f, b / 255.f, a / 255.f }; }

enum GameState { PLAYING, GAMEOVER };
enum Theme { SPACE, CYBERPUNK, SUNSET };

struct Vec2 { float x, y; };

struct Obj {               // falling object (all are circles, AABB collision like the SFML version)
    Vec2 pos;              // top-left of bounding box
    float r;
};

struct FloatingText { std::string s; Vec2 pos; Col col; float life; };
struct Particle { Vec2 pos; Vec2 vel; Col col; float life; };
struct BgStar { Vec2 pos; float r; float a; };

// ---------------------------------------------------------------- globals
static GLuint gProg = 0, gVao = 0, gVbo = 0;
static GLint uPos, uSize, uColor, uColor2, uShape, uRes;

static GameState gState = PLAYING;
static Theme gTheme = SPACE;
static bool gRestart = false;

// ---------------------------------------------------------------- shaders
static const char* VS = R"(#version 330 core
layout(location = 0) in vec2 aPos;      // unit quad 0..1
uniform vec2 uPos;                      // pixels, top-left
uniform vec2 uSize;                     // pixels
uniform vec2 uRes;                      // logical resolution
out vec2 vUV;
void main() {
    vec2 p = uPos + aPos * uSize;
    gl_Position = vec4(p.x / uRes.x * 2.0 - 1.0, 1.0 - p.y / uRes.y * 2.0, 0.0, 1.0);
    vUV = aPos;
}
)";

static const char* FS = R"(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform vec4 uColor;
uniform vec4 uColor2;
uniform int uShape;                     // 0 rect, 1 circle, 2 vertical gradient
void main() {
    if (uShape == 1) {
        float d = length(vUV - vec2(0.5));
        if (d > 0.5) discard;
        float a = smoothstep(0.5, 0.44, d);
        FragColor = vec4(uColor.rgb, uColor.a * a);
    } else if (uShape == 2) {
        FragColor = mix(uColor, uColor2, vUV.y);
    } else {
        FragColor = uColor;
    }
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::fprintf(stderr, "Shader error: %s\n", log);
        std::exit(1);
    }
    return s;
}

static void initGL() {
    GLuint vs = compile(GL_VERTEX_SHADER, VS);
    GLuint fs = compile(GL_FRAGMENT_SHADER, FS);
    gProg = glCreateProgram();
    glAttachShader(gProg, vs);
    glAttachShader(gProg, fs);
    glLinkProgram(gProg);
    GLint ok = 0;
    glGetProgramiv(gProg, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(gProg, 1024, nullptr, log);
        std::fprintf(stderr, "Link error: %s\n", log);
        std::exit(1);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    uPos = glGetUniformLocation(gProg, "uPos");
    uSize = glGetUniformLocation(gProg, "uSize");
    uColor = glGetUniformLocation(gProg, "uColor");
    uColor2 = glGetUniformLocation(gProg, "uColor2");
    uShape = glGetUniformLocation(gProg, "uShape");
    uRes = glGetUniformLocation(gProg, "uRes");

    const float quad[] = { 0, 0, 1, 0, 1, 1,   0, 0, 1, 1, 0, 1 };
    glGenVertexArrays(1, &gVao);
    glGenBuffers(1, &gVbo);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ---------------------------------------------------------------- draw primitives
static void drawShape(float x, float y, float w, float h, Col c, int shape, Col c2 = { 0, 0, 0, 1 }) {
    glUniform2f(uPos, x, y);
    glUniform2f(uSize, w, h);
    glUniform4f(uColor, c.r, c.g, c.b, c.a);
    glUniform4f(uColor2, c2.r, c2.g, c2.b, c2.a);
    glUniform1i(uShape, shape);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
static void drawRect(float x, float y, float w, float h, Col c) { drawShape(x, y, w, h, c, 0); }
static void drawCircle(float x, float y, float r, Col c) { drawShape(x, y, r * 2, r * 2, c, 1); }

// ---------------------------------------------------------------- bitmap font 5x7
struct Glyph { char c; const char* row[7]; };
static const Glyph FONT[] = {
    {'A', {"01110","10001","10001","11111","10001","10001","10001"}},
    {'B', {"11110","10001","10001","11110","10001","10001","11110"}},
    {'C', {"01110","10001","10000","10000","10000","10001","01110"}},
    {'D', {"11110","10001","10001","10001","10001","10001","11110"}},
    {'E', {"11111","10000","10000","11110","10000","10000","11111"}},
    {'F', {"11111","10000","10000","11110","10000","10000","10000"}},
    {'G', {"01110","10001","10000","10111","10001","10001","01111"}},
    {'H', {"10001","10001","10001","11111","10001","10001","10001"}},
    {'I', {"01110","00100","00100","00100","00100","00100","01110"}},
    {'J', {"00111","00010","00010","00010","00010","10010","01100"}},
    {'K', {"10001","10010","10100","11000","10100","10010","10001"}},
    {'L', {"10000","10000","10000","10000","10000","10000","11111"}},
    {'M', {"10001","11011","10101","10101","10001","10001","10001"}},
    {'N', {"10001","11001","10101","10011","10001","10001","10001"}},
    {'O', {"01110","10001","10001","10001","10001","10001","01110"}},
    {'P', {"11110","10001","10001","11110","10000","10000","10000"}},
    {'Q', {"01110","10001","10001","10001","10101","10010","01101"}},
    {'R', {"11110","10001","10001","11110","10100","10010","10001"}},
    {'S', {"01111","10000","10000","01110","00001","00001","11110"}},
    {'T', {"11111","00100","00100","00100","00100","00100","00100"}},
    {'U', {"10001","10001","10001","10001","10001","10001","01110"}},
    {'V', {"10001","10001","10001","10001","10001","01010","00100"}},
    {'W', {"10001","10001","10001","10101","10101","11011","10001"}},
    {'X', {"10001","10001","01010","00100","01010","10001","10001"}},
    {'Y', {"10001","10001","01010","00100","00100","00100","00100"}},
    {'Z', {"11111","00001","00010","00100","01000","10000","11111"}},
    {'0', {"01110","10001","10011","10101","11001","10001","01110"}},
    {'1', {"00100","01100","00100","00100","00100","00100","01110"}},
    {'2', {"01110","10001","00001","00010","00100","01000","11111"}},
    {'3', {"11110","00001","00001","01110","00001","00001","11110"}},
    {'4', {"00010","00110","01010","10010","11111","00010","00010"}},
    {'5', {"11111","10000","11110","00001","00001","10001","01110"}},
    {'6', {"00110","01000","10000","11110","10001","10001","01110"}},
    {'7', {"11111","00001","00010","00100","01000","01000","01000"}},
    {'8', {"01110","10001","10001","01110","10001","10001","01110"}},
    {'9', {"01110","10001","10001","01111","00001","00010","01100"}},
    {'+', {"00000","00100","00100","11111","00100","00100","00000"}},
    {'-', {"00000","00000","00000","11111","00000","00000","00000"}},
    {':', {"00000","00100","00100","00000","00100","00100","00000"}},
    {'!', {"00100","00100","00100","00100","00100","00000","00100"}},
    {'\'',{"00100","00100","01000","00000","00000","00000","00000"}},
    {'(', {"00010","00100","01000","01000","01000","00100","00010"}},
    {')', {"01000","00100","00010","00010","00010","00100","01000"}},
};

static const Glyph* findGlyph(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    for (const Glyph& g : FONT) if (g.c == c) return &g;
    return nullptr;
}

static float textWidth(const std::string& s, float px) { return s.size() * 6.f * px; }

static void drawText(const std::string& s, float x, float y, float px, Col col) {
    float cx = x;
    for (char ch : s) {
        const Glyph* g = findGlyph(ch);
        if (g) {
            for (int r = 0; r < 7; ++r)
                for (int c = 0; c < 5; ++c)
                    if (g->row[r][c] == '1')
                        drawRect(cx + c * px, y + r * px, px, px, col);
        }
        cx += 6.f * px;
    }
}

// ---------------------------------------------------------------- input
static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_B) gTheme = static_cast<Theme>((gTheme + 1) % 3);
    if (gState == GAMEOVER) {
        if (key == GLFW_KEY_R) gRestart = true;
        else if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, GLFW_TRUE);
    }
}

// ---------------------------------------------------------------- helpers
static int rnd(int n) { return std::rand() % n; }
static float rndX() { return static_cast<float>(rnd(740)); }
static bool hit(float px, float py, float pw, float ph, const Obj& o) {
    float d = o.r * 2.f;
    return px < o.pos.x + d && px + pw > o.pos.x && py < o.pos.y + d && py + ph > o.pos.y;
}

// ---------------------------------------------------------------- main
int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    if (!glfwInit()) { std::fprintf(stderr, "GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(W, H, "Star Catcher Ultimate - OpenGL", nullptr, nullptr);
    if (!window) { std::fprintf(stderr, "Window creation failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, keyCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::fprintf(stderr, "GLEW init failed\n"); return 1; }
    initGL();

    // background stars
    std::vector<BgStar> bgStars;
    for (int i = 0; i < 60; ++i)
        bgStars.push_back({ { (float)rnd(800), (float)rnd(600) }, (float)(rnd(3) + 1), (rnd(150) + 50) / 255.f });

    // player
    const Vec2 defaultSize = { 130.f, 22.f };
    Vec2 pSize = defaultSize;
    Vec2 pPos = { 335.f, 540.f };
    const float playerSpeed = 12.f;
    Col pOutline = C(255, 255, 255);
    float pOutlineT = 3.f;

    bool shield = false, shrink = false;
    double shieldStart = 0, shrinkStart = 0;
    const float shieldDur = 7.f, shrinkDur = 5.f;

    Obj star   = { { rndX(), -20.f },  14.f };
    Obj meteor = { { rndX(), -300.f }, 16.f };
    Obj trap   = { { rndX(), -500.f }, 13.f };
    Obj bonus  = { { 0, 0 }, 12.f };
    float starSpeed = 4.5f, meteorSpeed = 5.2f, trapSpeed = 4.8f, bonusSpeed = 4.0f;
    int bonusType = 0;
    bool bonusActive = false;

    int score = 0, lives = 3, combo = 0, highscore = 0;
    {
        std::ifstream f("highscore.txt");
        if (f.is_open()) f >> highscore;
    }

    std::vector<FloatingText> texts;
    std::vector<Particle> particles;

    auto spawnParticles = [&](Vec2 pos, Col c) {
        for (int i = 0; i < 12; ++i) {
            float ang = (rnd(360)) * 3.14159f / 180.f;
            float spd = (float)(rnd(5) + 2);
            particles.push_back({ pos, { std::cos(ang) * spd, std::sin(ang) * spd }, c, 1.f });
        }
    };
    auto addText = [&](const std::string& s, Vec2 pos, Col c) { texts.push_back({ s, pos, c, 1.f }); };

    double last = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - last);
        last = now;
        if (dt > 0.05f) dt = 0.05f;      // үлкен секірістерден қорғау
        const float k = dt * 60.f;       // 1.0 = дәл 60 FPS-тегі бастапқы жылдамдық

        glfwPollEvents();

        if (gRestart) {
            gRestart = false;
            score = 0; lives = 3; combo = 0;
            starSpeed = 4.5f;
            shield = shrink = false;
            pSize = defaultSize;
            pOutline = C(255, 255, 255);
            pOutlineT = 3.f;
            star.pos = { rndX(), -20.f };
            meteor.pos = { rndX(), -400.f };
            trap.pos = { rndX(), -500.f };
            gState = PLAYING;
        }

        // ------------------------------------------------ update
        if (gState == PLAYING) {
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                if (pPos.x > 10) pPos.x -= playerSpeed * k;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                if (pPos.x < 800 - pSize.x - 10) pPos.x += playerSpeed * k;

            star.pos.y += starSpeed * k;
            meteor.pos.y += meteorSpeed * k;
            trap.pos.y += trapSpeed * k;

            if (bonusActive) {
                bonus.pos.y += bonusSpeed * k;
                if (bonus.pos.y > 600.f) bonusActive = false;
            } else if (rnd(350) == 1) {
                bonusActive = true;
                bonusType = rnd(2) + 1;
                bonus.pos = { rndX(), -20.f };
            }

            if (shield && now - shieldStart > shieldDur) {
                shield = false;
                pOutline = C(255, 255, 255);
                pOutlineT = 3.f;
            }
            if (shrink && now - shrinkStart > shrinkDur) {
                shrink = false;
                pSize = defaultSize;
            }

            if (star.pos.y > 600.f) {
                lives--; combo = 0;
                addText("-1 LIFE", { star.pos.x, 520.f }, C(255, 0, 0));
                star.pos = { rndX(), -20.f };
            }
            if (meteor.pos.y > 600.f) meteor.pos = { rndX(), -(float)(rnd(300) + 100) };
            if (trap.pos.y > 600.f)   trap.pos   = { rndX(), -(float)(rnd(400) + 200) };

            // star
            if (hit(pPos.x, pPos.y, pSize.x, pSize.y, star)) {
                combo++;
                int pts = 10 * combo;
                score += pts;
                spawnParticles(star.pos, C(255, 255, 0));
                addText("+" + std::to_string(pts) + " (x" + std::to_string(combo) + ")", star.pos, C(255, 255, 0));
                star.pos = { rndX(), -20.f };
                if (starSpeed < 10.f) starSpeed += 0.2f;
            }
            // meteor
            if (hit(pPos.x, pPos.y, pSize.x, pSize.y, meteor)) {
                if (!shield) {
                    lives--; combo = 0;
                    spawnParticles(meteor.pos, C(255, 0, 0));
                    addText("HIT!", meteor.pos, C(255, 0, 0));
                } else {
                    spawnParticles(meteor.pos, C(0, 150, 255));
                    addText("BLOCKED!", meteor.pos, C(0, 150, 255));
                }
                meteor.pos = { rndX(), -(float)(rnd(300) + 200) };
            }
            // trap star
            if (hit(pPos.x, pPos.y, pSize.x, pSize.y, trap)) {
                shrink = true;
                shrinkStart = now;
                pSize = { 70.f, 22.f };
                spawnParticles(trap.pos, C(255, 255, 255));
                addText("SHRINK!", trap.pos, C(255, 255, 255));
                trap.pos = { rndX(), -(float)(rnd(400) + 200) };
            }
            // bonus
            if (bonusActive && hit(pPos.x, pPos.y, pSize.x, pSize.y, bonus)) {
                if (bonusType == 1) {
                    lives++;
                    addText("+1 LIFE!", bonus.pos, C(0, 255, 0));
                    spawnParticles(bonus.pos, C(0, 255, 0));
                } else {
                    shield = true;
                    shieldStart = now;
                    pOutline = C(0, 150, 255);
                    pOutlineT = 5.f;
                    addText("SHIELD!", bonus.pos, C(0, 150, 255));
                    spawnParticles(bonus.pos, C(0, 150, 255));
                }
                bonusActive = false;
            }

            if (lives <= 0) {
                if (score > highscore) {
                    highscore = score;
                    std::ofstream out("highscore.txt");
                    out << highscore;
                }
                gState = GAMEOVER;
            }

            for (size_t i = 0; i < particles.size();) {
                particles[i].pos.x += particles[i].vel.x * k;
                particles[i].pos.y += particles[i].vel.y * k;
                particles[i].life -= dt;
                if (particles[i].life <= 0) particles.erase(particles.begin() + i); else ++i;
            }
            for (size_t i = 0; i < texts.size();) {
                texts[i].pos.y -= 1.5f * k;
                texts[i].life -= dt;
                if (texts[i].life <= 0) texts.erase(texts.begin() + i); else ++i;
            }
        }

        // ------------------------------------------------ render
        glViewport(0, 0, W, H);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(gProg);
        glUniform2f(uRes, (float)W, (float)H);
        glBindVertexArray(gVao);

        // background gradient (shader mode 2)
        Col top, bottom;
        if (gTheme == SPACE)          { top = C(10, 12, 24);  bottom = C(25, 20, 50); }
        else if (gTheme == CYBERPUNK) { top = C(20, 5, 25);   bottom = C(60, 10, 70); }
        else                          { top = C(40, 20, 10);  bottom = C(110, 45, 25); }
        drawShape(0, 0, (float)W, (float)H, top, 2, bottom);

        for (const BgStar& s : bgStars) drawCircle(s.pos.x, s.pos.y, s.r, { 1, 1, 1, s.a });
        for (const Particle& p : particles) {
            Col c = p.col; c.a = p.life > 1.f ? 1.f : p.life;
            drawCircle(p.pos.x, p.pos.y, 3.f, c);
        }

        // player: outline then fill
        drawRect(pPos.x - pOutlineT, pPos.y - pOutlineT, pSize.x + 2 * pOutlineT, pSize.y + 2 * pOutlineT, pOutline);
        drawRect(pPos.x, pPos.y, pSize.x, pSize.y, C(0, 230, 255));

        drawCircle(star.pos.x, star.pos.y, star.r, C(255, 215, 0));
        drawCircle(meteor.pos.x, meteor.pos.y, meteor.r, C(255, 50, 50));
        drawCircle(trap.pos.x, trap.pos.y, trap.r, C(255, 255, 255));
        if (bonusActive)
            drawCircle(bonus.pos.x, bonus.pos.y, bonus.r, bonusType == 1 ? C(0, 255, 0) : C(0, 150, 255));

        for (const FloatingText& ft : texts) {
            Col c = ft.col; c.a = ft.life > 1.f ? 1.f : ft.life;
            drawText(ft.s, ft.pos.x, ft.pos.y, 2.f, c);
        }

        // UI bar
        drawRect(18.f, 13.f, 764.f, 49.f, C(0, 230, 255, 120));
        drawRect(20.f, 15.f, 760.f, 45.f, C(15, 20, 35, 220));
        std::string ui = "SCR:" + std::to_string(score) + " LVS:" + std::to_string(lives) +
                         " COMBO:x" + std::to_string(combo) + " HIGH:" + std::to_string(highscore);
        if (shield) {
            int rem = (int)(shieldDur - (now - shieldStart)); if (rem < 0) rem = 0;
            ui += " SHLD:" + std::to_string(rem) + "s";
        }
        if (shrink) {
            int rem = (int)(shrinkDur - (now - shrinkStart)); if (rem < 0) rem = 0;
            ui += " SHRINK:" + std::to_string(rem) + "s";
        }
        drawText(ui, 28.f, 30.f, 2.f, C(255, 255, 255));
        drawText("PRESS 'B' TO CHANGE BACKGROUND THEME", 25.f, 575.f, 2.f, C(200, 200, 200, 150));

        if (gState == GAMEOVER) {
            drawRect(0, 0, (float)W, (float)H, C(0, 0, 0, 200));
            const std::string t1 = "GAME OVER";
            const std::string t2 = "PRESS 'R' TO RESTART";
            const std::string t3 = "PRESS 'ESC' TO EXIT";
            drawText(t1, (W - textWidth(t1, 6.f)) / 2.f, 200.f, 6.f, C(255, 0, 0));
            drawText(t2, (W - textWidth(t2, 3.f)) / 2.f, 300.f, 3.f, C(255, 80, 80));
            drawText(t3, (W - textWidth(t3, 3.f)) / 2.f, 340.f, 3.f, C(255, 80, 80));
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    glDeleteBuffers(1, &gVbo);
    glDeleteVertexArrays(1, &gVao);
    glDeleteProgram(gProg);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
