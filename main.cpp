// Room with smoother FPS camera, horror lighting (flicker + spotlight + fog),
// SOIL2 textures (floor, walls, ceiling, table, chairs, painting, Earth),
// animation (bulb sway + rotating Earth), perspective/ortho, and antialiasing.

#include <math.h>
#include <glut.h>    // Use FreeGLUT for *Up callbacks
#include <SOIL2.h>
#include <stdio.h>

// ---------------- Window & projection ----------------
int   win_posx = 100, win_posy = 100;
int   win_width = 960, win_height = 600;
float z_near = 0.1f, z_far = 100.0f;
float fovy = 60.0f;         // perspective FOV in degrees
int   use_perspective = 1;  // P toggles; else orthographic
float ortho_scale = 3.5f;   // "zoom" in orthographic

// ---------------- Room dimensions ----------------
const float ROOM_W = 8.0f;  // X
const float ROOM_D = 8.0f;  // Z
const float ROOM_H = 3.0f;  // Y

// ---------------- Camera (FPS-style, smoothed) ----------------
float eyeX = 3.0f, eyeY = 1.2f, eyeZ = 3.5f;
float yawDeg = -135.0f;
float pitchDeg = -8.0f;

// movement smoothing
float velX = 0, velY = 0, velZ = 0;   // camera velocity (world)
float maxSpeed = 2.2f;          // units/s (base)
float accel = 8.0f;          // approach speed (1/s)
float damping = 6.0f;          // extra damping when no input (1/s)
int   boostActive = 0;          // SHIFT boosts speed

// look smoothing
float lookSpeed = 85.0f;        // deg/s when holding arrow

// computed every frame (forward/right/up unit vectors)
float fwdX = 0, fwdY = 0, fwdZ = -1;
float rgtX = 1, rgtY = 0, rgtZ = 0;
float upX = 0, upY = 1, upZ = 0;

// ---------------- Key state (renamed to avoid collision) ----------------
int gKeyDown[256] = { 0 };
int gSpecialKeyDown[512] = { 0 };

// ---------------- Animation ----------------
int   animate_on = 1;
float timeSec = 0.0f;
int   lastTimeMS = 0;
float earthAngle = 0.0f; // rotation of the textured Earth

// ---------------- Toggles ----------------
int showAxes = 0;

// ---------------- Textures (SOIL2) ----------------
GLuint texFloor = 0, texWall = 0, texCeil = 0, texWood = 0, texPainting = 0, texEarth = 0;

// ---------------- Utilities ----------------
#define DEG2RAD 0.017453292519943295769f
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float len3(float x, float y, float z) { return sqrtf(x * x + y * y + z * z); }
static void  norm3(float* x, float* y, float* z) { float L = len3(*x, *y, *z); if (L > 0) { *x /= L; *y /= L; *z /= L; } }
static void cross3(float ax, float ay, float az, float bx, float by, float bz, float* rx, float* ry, float* rz) {
    *rx = ay * bz - az * by; *ry = az * bx - ax * bz; *rz = ax * by - ay * bx;
}
static float fractf(float x) { return x - floorf(x); }

static GLuint loadTextureSOIL(const char* file, int invertY) {
    int flags = SOIL_FLAG_MIPMAPS | (invertY ? SOIL_FLAG_INVERT_Y : 0);
    GLuint id = SOIL_load_OGL_texture(file, SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, flags);
    if (!id) {
        printf("SOIL2: failed to load '%s' : %s\n", file, SOIL_last_result());
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
    printf("SOIL2: loaded '%s' (id=%u)\n", file, id);
    return id;
}

// ---------------- Text overlay ----------------
void renderBitmapString(float x, float y, void* font, const char* s) {
    glRasterPos2f(x, y);
    while (*s) glutBitmapCharacter(font, *s++);
}
void displayLabel() {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, win_width, 0, win_height);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 0.85f);
    void* font = GLUT_BITMAP_8_BY_13;
    float x = 10.0f, y = win_height - 18.0f, lh = 16.0f;
    renderBitmapString(x, y, font, "W/S: forward/back  A/D: strafe  Q/E: up/down  Arrow: look  Shift: faster");
    renderBitmapString(x, y -= lh, font, "P: persp/ortho  Z/X: zoom  M: anim  T: axes  R: reset  ESC: quit");

    glEnable(GL_LIGHTING); glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

// ---------------- Axes ----------------
void axes() {
    if (!showAxes) return;
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(2, 0, 0);
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, 2, 0);
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, 2);
    glEnd();
    glEnable(GL_LIGHTING);
}

// ---------------- Primitive helpers ----------------
static void drawBox(float sx, float sy, float sz) {
    glPushMatrix(); glScalef(sx, sy, sz); glutSolidCube(1.0f); glPopMatrix();
}
static void drawTexturedBox(float sx, float sy, float sz, float tileU, float tileV) {
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    glBegin(GL_QUADS);
    // +X
    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0);          glVertex3f(hx, -hy, -hz);
    glTexCoord2f(tileU, 0);      glVertex3f(hx, -hy, hz);
    glTexCoord2f(tileU, tileV);  glVertex3f(hx, hy, hz);
    glTexCoord2f(0, tileV);   glVertex3f(hx, hy, -hz);
    // -X
    glNormal3f(-1, 0, 0);
    glTexCoord2f(0, 0);          glVertex3f(-hx, -hy, hz);
    glTexCoord2f(tileU, 0);      glVertex3f(-hx, -hy, -hz);
    glTexCoord2f(tileU, tileV);  glVertex3f(-hx, hy, -hz);
    glTexCoord2f(0, tileV);   glVertex3f(-hx, hy, hz);
    // +Y
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);          glVertex3f(-hx, hy, -hz);
    glTexCoord2f(tileU, 0);      glVertex3f(hx, hy, -hz);
    glTexCoord2f(tileU, tileV);  glVertex3f(hx, hy, hz);
    glTexCoord2f(0, tileV);   glVertex3f(-hx, hy, hz);
    // -Y
    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0);          glVertex3f(-hx, -hy, hz);
    glTexCoord2f(tileU, 0);      glVertex3f(hx, -hy, hz);
    glTexCoord2f(tileU, tileV);  glVertex3f(hx, -hy, -hz);
    glTexCoord2f(0, tileV);   glVertex3f(-hx, -hy, -hz);
    // +Z
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0);          glVertex3f(hx, -hy, hz);
    glTexCoord2f(tileU, 0);      glVertex3f(-hx, -hy, hz);
    glTexCoord2f(tileU, tileV);  glVertex3f(-hx, hy, hz);
    glTexCoord2f(0, tileV);   glVertex3f(hx, hy, hz);
    // -Z
    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0);          glVertex3f(-hx, -hy, -hz);
    glTexCoord2f(tileU, 0);      glVertex3f(hx, -hy, -hz);
    glTexCoord2f(tileU, tileV);  glVertex3f(hx, hy, -hz);
    glTexCoord2f(0, tileV);   glVertex3f(-hx, hy, -hz);
    glEnd();
}

// ---------------- Room (textured floor/walls/ceiling + painting) ----------------
void drawRoom() {
    const float x0 = -ROOM_W * 0.5f, x1 = ROOM_W * 0.5f;
    const float z0 = -ROOM_D * 0.5f, z1 = ROOM_D * 0.5f;
    const float y0 = 0.0f, y1 = ROOM_H;

    glDisable(GL_CULL_FACE);

    // Floor
    glColor3f(1, 1, 1);
    if (texFloor) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texFloor);
    }
    float tile = 8.0f;
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);

    if (texFloor) glTexCoord2f(0, 0);
    glVertex3f(x0, y0, z0);
    if (texFloor) glTexCoord2f(tile, 0);
    glVertex3f(x1, y0, z0);
    if (texFloor) glTexCoord2f(tile, tile);
    glVertex3f(x1, y0, z1);
    if (texFloor) glTexCoord2f(0, tile);
    glVertex3f(x0, y0, z1);

    glEnd();
    if (texFloor) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    // Ceiling (textured)
    glColor3f(1, 1, 1);
    if (texCeil) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texCeil);
    }
    float ceilU = 4.0f, ceilV = 4.0f;
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);

    if (texCeil) glTexCoord2f(0, 0);
    glVertex3f(x0, y1, z0);
    if (texCeil) glTexCoord2f(ceilU, 0);
    glVertex3f(x0, y1, z1);
    if (texCeil) glTexCoord2f(ceilU, ceilV);
    glVertex3f(x1, y1, z1);
    if (texCeil) glTexCoord2f(0, ceilV);
    glVertex3f(x1, y1, z0);

    glEnd();
    if (texCeil) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    // Walls
    glColor3f(1, 1, 1);
    if (texWall) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texWall);
    }
    float wallU = 4.0f, wallV = 2.0f;
    glBegin(GL_QUADS);

    // +X wall
    glNormal3f(-1, 0, 0);
    if (texWall) glTexCoord2f(0, 0);
    glVertex3f(x1, y0, z0);
    if (texWall) glTexCoord2f(wallU, 0);
    glVertex3f(x1, y0, z1);
    if (texWall) glTexCoord2f(wallU, wallV);
    glVertex3f(x1, y1, z1);
    if (texWall) glTexCoord2f(0, wallV);
    glVertex3f(x1, y1, z0);

    // -X wall
    glNormal3f(1, 0, 0);
    if (texWall) glTexCoord2f(0, 0);
    glVertex3f(x0, y0, z1);
    if (texWall) glTexCoord2f(wallU, 0);
    glVertex3f(x0, y0, z0);
    if (texWall) glTexCoord2f(wallU, wallV);
    glVertex3f(x0, y1, z0);
    if (texWall) glTexCoord2f(0, wallV);
    glVertex3f(x0, y1, z1);

    // +Z wall
    glNormal3f(0, 0, -1);
    if (texWall) glTexCoord2f(0, 0);
    glVertex3f(x0, y0, z1);
    if (texWall) glTexCoord2f(wallU, 0);
    glVertex3f(x1, y0, z1);
    if (texWall) glTexCoord2f(wallU, wallV);
    glVertex3f(x1, y1, z1);
    if (texWall) glTexCoord2f(0, wallV);
    glVertex3f(x0, y1, z1);

    // -Z wall (front)
    glNormal3f(0, 0, 1);
    if (texWall) glTexCoord2f(0, 0);
    glVertex3f(x1, y0, z0);
    if (texWall) glTexCoord2f(wallU, 0);
    glVertex3f(x0, y0, z0);
    if (texWall) glTexCoord2f(wallU, wallV);
    glVertex3f(x0, y1, z0);
    if (texWall) glTexCoord2f(0, wallV);
    glVertex3f(x1, y1, z0);

    glEnd();
    if (texWall) {
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    // Painting on -Z wall
    if (texPainting) {
        float pw = 1.4f, ph = 0.9f, z = z0 + 0.001f, y = 1.6f;
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texPainting);
        glColor3f(1, 1, 1);
        glBegin(GL_QUADS);
        glNormal3f(0, 0, 1);
        glTexCoord2f(0, 0); glVertex3f(-pw * 0.5f, y - ph * 0.5f, z);
        glTexCoord2f(1, 0); glVertex3f(pw * 0.5f, y - ph * 0.5f, z);
        glTexCoord2f(1, 1); glVertex3f(pw * 0.5f, y + ph * 0.5f, z);
        glTexCoord2f(0, 1); glVertex3f(-pw * 0.5f, y + ph * 0.5f, z);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);

        // frame
        glColor3f(0.25f, 0.15f, 0.08f);
        float t = 0.03f;
        glBegin(GL_QUADS);
        // bottom
        glVertex3f(-pw * 0.5f - t, y - ph * 0.5f - t, z);
        glVertex3f(pw * 0.5f + t, y - ph * 0.5f - t, z);
        glVertex3f(pw * 0.5f + t, y - ph * 0.5f, z);
        glVertex3f(-pw * 0.5f - t, y - ph * 0.5f, z);
        // top
        glVertex3f(-pw * 0.5f - t, y + ph * 0.5f, z);
        glVertex3f(pw * 0.5f + t, y + ph * 0.5f, z);
        glVertex3f(pw * 0.5f + t, y + ph * 0.5f + t, z);
        glVertex3f(-pw * 0.5f - t, y + ph * 0.5f + t, z);
        // left
        glVertex3f(-pw * 0.5f - t, y - ph * 0.5f, z);
        glVertex3f(-pw * 0.5f, y - ph * 0.5f, z);
        glVertex3f(-pw * 0.5f, y + ph * 0.5f, z);
        glVertex3f(-pw * 0.5f - t, y + ph * 0.5f, z);
        // right
        glVertex3f(pw * 0.5f, y - ph * 0.5f, z);
        glVertex3f(pw * 0.5f + t, y - ph * 0.5f, z);
        glVertex3f(pw * 0.5f + t, y + ph * 0.5f, z);
        glVertex3f(pw * 0.5f, y + ph * 0.5f, z);
        glEnd();
    }
}

// ---------------- Furniture (table + textured chairs) ----------------
void drawTable() {
    const float topW = 1.20f, topD = 0.80f, topT = 0.08f;
    const float height = 0.75f;
    const float legT = 0.08f;
    const float legH = height - topT * 0.5f;

    // top
    if (texWood) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texWood); glColor3f(1, 1, 1); }
    else { glColor3f(0.55f, 0.34f, 0.20f); }
    glPushMatrix();
    glTranslatef(0.0f, height, 0.0f);
    if (texWood) drawTexturedBox(topW, topT, topD, 1.5f, 1.0f); else drawBox(topW, topT, topD);
    glPopMatrix();
    if (texWood) { glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D); }

    // legs (also textured)
    if (texWood) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texWood); glColor3f(1, 1, 1); }
    else { glColor3f(0.48f, 0.29f, 0.16f); }
    const float halfW = topW * 0.5f - legT * 0.5f;
    const float halfD = topD * 0.5f - legT * 0.5f;
    const float y = legH * 0.5f;
    if (texWood) {
        glPushMatrix(); glTranslatef(halfW, y, halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(halfW, y, -halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
        glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D);
    }
    else {
        glPushMatrix(); glTranslatef(halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
        glPushMatrix(); glTranslatef(halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
    }
}

void drawChair() {
    const float seatW = 0.45f, seatD = 0.45f, seatT = 0.06f;
    const float seatH = 0.45f;
    const float legT = 0.06f;
    const float backH = 0.45f;

    if (texWood) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texWood); glColor3f(1, 1, 1); }
    else { glColor3f(0.60f, 0.36f, 0.22f); }
    // seat
    glPushMatrix(); glTranslatef(0.0f, seatH, 0.0f);
    if (texWood) drawTexturedBox(seatW, seatT, seatD, 1.0f, 1.0f); else drawBox(seatW, seatT, seatD);
    glPopMatrix();

    // legs
    if (!texWood) glColor3f(0.50f, 0.30f, 0.18f);
    const float legH = seatH - seatT * 0.5f;
    const float halfW = seatW * 0.5f - legT * 0.5f;
    const float halfD = seatD * 0.5f - legT * 0.5f;
    const float y = legH * 0.5f;
    if (texWood) {
        glPushMatrix(); glTranslatef(halfW, y, halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(halfW, y, -halfD); drawTexturedBox(legT, legH, legT, 1.0f, 1.0f); glPopMatrix();
    }
    else {
        glPushMatrix(); glTranslatef(halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
        glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
        glPushMatrix(); glTranslatef(halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
    }

    // backrest
    if (!texWood) glColor3f(0.58f, 0.34f, 0.20f);
    glPushMatrix(); glTranslatef(0.0f, seatH + backH * 0.5f, -seatD * 0.5f + legT * 0.5f);
    if (texWood) drawTexturedBox(seatW, backH, legT, 1.0f, 1.0f); else drawBox(seatW, backH, legT);
    glPopMatrix();

    if (texWood) { glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D); }
}

// Earth (textured GLU sphere)
void drawTexturedEarth(float radius) {
    if (!texEarth) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texEarth);

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);

    glColor3f(1, 1, 1);
    glPushMatrix();
    glRotatef(earthAngle, 0, 1, 0);
    gluSphere(quad, radius, 64, 64);
    glPopMatrix();

    gluDeleteQuadric(quad);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

// ---------------- Horror lights ----------------
float g_flicker = 10;
float computeFlicker(float t) {
    float s = 0.5f * (sinf(7.0f * t) + sinf(13.0f * t + 1.3f));
    float base = 0.75f + 0.25f * s; // 0.5..1.0
    float n = fractf(sinf(47.0f * t) * 125.0f);
    float drop = (n < 0.035f) ? 0.35f : 1.0f;  // occasional dip
    return clampf(base * drop, 0.15f, 1.0f);
}

void drawBulbLampAndLight() {
    const float anchorY = ROOM_H - 0.05f;
    const float cordLen = 0.28f;

    float sway = animate_on ? 10.0f * sinf(timeSec * 1.4f) : 0.0f;

    // cord
    glColor3f(0.2f, 0.2f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, anchorY, 0.0f);
    glRotatef(sway, 0.0f, 0.0f, 1.0f);
    glTranslatef(0.0f, -cordLen * 0.5f, 0.0f);
    drawBox(0.02f, cordLen, 0.02f);
    glPopMatrix();

    // bulb + light0 position
    GLfloat emit[4] = { 1.0f * g_flicker, 0.96f * g_flicker, 0.85f * g_flicker, 1.0f };
    GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glPushMatrix();
    glTranslatef(0.0f, anchorY, 0.0f);
    glRotatef(sway, 0.0f, 0.0f, 1.0f);
    glTranslatef(0.0f, -cordLen, 0.0f);

    GLfloat Lpos[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, Lpos);

    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emit);
    glColor3f(1.0f, 1.0f, 0.85f);
    glutSolidSphere(0.08f, 24, 24);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, zero);

    glColor3f(0.85f, 0.82f, 0.78f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidTorus(0.025f, 0.16f, 24, 48);
    glPopMatrix();
}

void setupHorrorLights() {
    // very low global ambient
    GLfloat Lmodel_amb[4] = { 0.03f, 0.03f, 0.035f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, Lmodel_amb);

    // light0: flickering warm bulb
    float f = g_flicker;
    GLfloat L0_dif[4] = { 1.00f * f, 0.88f * f, 0.60f * f, 1.0f };
    GLfloat L0_spe[4] = { 0.90f * f, 0.85f * f, 0.80f * f, 1.0f };
    GLfloat L0_amb[4] = { 0.05f * f, 0.045f * f, 0.03f * f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, L0_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, L0_dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, L0_spe);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.06f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.025f);

    // light1: narrow red spotlight from -Z wall
    glEnable(GL_LIGHT1);
    GLfloat L1_pos[4] = { 0.0f, 1.6f, -ROOM_D * 0.5f + 0.2f, 1.0f };
    GLfloat L1_dir[3] = { 0.0f, -0.1f, 1.0f };
    GLfloat L1_dif[4] = { 0.55f, 0.05f, 0.05f, 1.0f };
    GLfloat L1_spe[4] = { 0.40f, 0.10f, 0.10f, 1.0f };
    GLfloat L1_amb[4] = { 0.02f, 0.00f, 0.00f, 1.0f };
    glLightfv(GL_LIGHT1, GL_POSITION, L1_pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, L1_dif);
    glLightfv(GL_LIGHT1, GL_SPECULAR, L1_spe);
    glLightfv(GL_LIGHT1, GL_AMBIENT, L1_amb);
    glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 20.0f);
    glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 32.0f);
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, L1_dir);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.04f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.02f);
}

// ---------------- Camera math ----------------
void updateCameraBasis() {
    float yawR = yawDeg * DEG2RAD;
    float pitR = pitchDeg * DEG2RAD;

    fwdX = cosf(pitR) * sinf(yawR);
    fwdY = sinf(pitR);
    fwdZ = -cosf(pitR) * cosf(yawR);
    norm3(&fwdX, &fwdY, &fwdZ);

    float rx, ry, rz;
    cross3(fwdX, fwdY, fwdZ, 0.0f, 1.0f, 0.0f, &rx, &ry, &rz);
    rgtX = rx; rgtY = ry; rgtZ = rz; norm3(&rgtX, &rgtY, &rgtZ);

    cross3(rgtX, rgtY, rgtZ, fwdX, fwdY, fwdZ, &upX, &upY, &upZ);
    norm3(&upX, &upY, &upZ);
}

void applyProjection() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (win_height == 0) ? 1.0f : (float)win_width / (float)win_height;
    if (use_perspective) {
        gluPerspective(fovy, aspect, z_near, z_far);
    }
    else {
        float h = ortho_scale;
        float w = ortho_scale * aspect;
        glOrtho(-w, w, -h, h, z_near, z_far);
    }
    glMatrixMode(GL_MODELVIEW);
}

// ---------------- Input (smoothed with key states) ----------------
static void updateBoostFromModifiers() {
    int mod = glutGetModifiers();
    boostActive = (mod & GLUT_ACTIVE_SHIFT) ? 1 : 0;
}

void keyboardDown(unsigned char key, int x, int y) {
    gKeyDown[(unsigned char)key] = 1;
    updateBoostFromModifiers();

    // one-shot actions
    switch (key) {
    case 'p': use_perspective = !use_perspective; applyProjection(); break;
    case 'z': if (use_perspective) { fovy = clampf(fovy - 2.0f, 20.0f, 90.0f); }
            else { ortho_scale = clampf(ortho_scale * 0.9f, 1.0f, 10.0f); } applyProjection(); break;
    case 'x': if (use_perspective) { fovy = clampf(fovy + 2.0f, 20.0f, 90.0f); }
            else { ortho_scale = clampf(ortho_scale / 0.9f, 1.0f, 10.0f); } applyProjection(); break;
    case 'm': animate_on = !animate_on; break;
    case 't': showAxes = !showAxes; break;
    case 'r': eyeX = 3.0f; eyeY = 1.2f; eyeZ = 3.5f; yawDeg = -135.0f; pitchDeg = -8.0f;
        fovy = 60.0f; ortho_scale = 3.5f; use_perspective = 1; velX = velY = velZ = 0; applyProjection(); break;
    case 27:  exit(0); // ESC
    }
    glutPostRedisplay();
}
void keyboardUp(unsigned char key, int x, int y) {
    gKeyDown[(unsigned char)key] = 0;
    updateBoostFromModifiers();
}
void onSpecialDown(int key, int x, int y) {
    gSpecialKeyDown[key] = 1;
    updateBoostFromModifiers();
}
void onSpecialUp(int key, int x, int y) {
    gSpecialKeyDown[key] = 0;
    updateBoostFromModifiers();
}

// ---------------- Display & idle ----------------
void placeChairsAroundTable() {
    const float tHalfW = 1.20f * 0.5f; // 0.60
    const float tHalfD = 0.80f * 0.5f; // 0.40
    const float seatHalf = 0.45f * 0.5f;
    const float gap = 0.25f;

    glPushMatrix(); glTranslatef(0.0f, 0.0f, -(tHalfD + gap + seatHalf));                    drawChair(); glPopMatrix();
    glPushMatrix(); glTranslatef(0.0f, 0.0f, (tHalfD + gap + seatHalf)); glRotatef(180, 0, 1, 0); drawChair(); glPopMatrix();
    glPushMatrix(); glTranslatef(-(tHalfW + gap + seatHalf), 0.0f, 0.0f); glRotatef(90, 0, 1, 0); drawChair(); glPopMatrix();
    glPushMatrix(); glTranslatef((tHalfW + gap + seatHalf), 0.0f, 0.0f); glRotatef(-90, 0, 1, 0); drawChair(); glPopMatrix();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // camera
    updateCameraBasis();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eyeX, eyeY, eyeZ, eyeX + fwdX, eyeY + fwdY, eyeZ + fwdZ, upX, upY, upZ);

    // lights (params updated per frame)
    setupHorrorLights();

    // scene
    drawRoom();
    axes();

    drawTable();
    placeChairsAroundTable();

    glPushMatrix();
    glTranslatef(0.35f, 0.90f, 0.05f); // Earth on table
    drawTexturedEarth(0.18f);
    glPopMatrix();

    drawBulbLampAndLight();

    displayLabel();
    glutSwapBuffers();
}

void idle() {
    int t = glutGet(GLUT_ELAPSED_TIME);
    if (lastTimeMS == 0) lastTimeMS = t;
    int dtMS = t - lastTimeMS;
    lastTimeMS = t;
    float dt = dtMS * 0.001f;

    if (animate_on) {
        timeSec += dt;
        earthAngle += 10.0f * dt;
        if (earthAngle >= 360.0f) earthAngle -= 360.0f;
    }

    // continuous look
    float yawRate = 0.0f, pitchRate = 0.0f;
    if (gSpecialKeyDown[GLUT_KEY_LEFT]) yawRate -= lookSpeed;
    if (gSpecialKeyDown[GLUT_KEY_RIGHT]) yawRate += lookSpeed;
    if (gSpecialKeyDown[GLUT_KEY_UP]) pitchRate += lookSpeed;
    if (gSpecialKeyDown[GLUT_KEY_DOWN]) pitchRate -= lookSpeed;
    yawDeg += yawRate * dt;
    pitchDeg = clampf(pitchDeg + pitchRate * dt, -89.0f, 89.0f);

    // smooth movement
    updateCameraBasis();
    int fw = (gKeyDown['w'] || gKeyDown['W']) ? 1 : 0;
    int bw = (gKeyDown['s'] || gKeyDown['S']) ? 1 : 0;
    int lf = (gKeyDown['a'] || gKeyDown['A']) ? 1 : 0;
    int rt = (gKeyDown['d'] || gKeyDown['D']) ? 1 : 0;
    int up = (gKeyDown['q'] || gKeyDown['Q']) ? 1 : 0;
    int dn = (gKeyDown['e'] || gKeyDown['E']) ? 1 : 0;

    float dx = fwdX * (fw - bw) + rgtX * (rt - lf);
    float dy = fwdY * (fw - bw) + (up - dn) * 1.0f;
    float dz = fwdZ * (fw - bw) + rgtZ * (rt - lf);
    float L = len3(dx, dy, dz);
    if (L > 0.0001f) { dx /= L; dy /= L; dz /= L; }

    float speed = maxSpeed * (boostActive ? 2.2f : 1.0f);
    float tx = dx * speed, ty = dy * speed, tz = dz * speed;

    velX += (tx - velX) * accel * dt;
    velY += (ty - velY) * accel * dt;
    velZ += (tz - velZ) * accel * dt;
    if (L < 0.0001f) { velX -= velX * damping * dt; velY -= velY * damping * dt; velZ -= velZ * damping * dt; }

    eyeX += velX * dt; eyeY += velY * dt; eyeZ += velZ * dt;

    // clamp inside room
    float margin = 0.25f;
    eyeX = clampf(eyeX, -ROOM_W * 0.5f + margin, ROOM_W * 0.5f - margin);
    eyeZ = clampf(eyeZ, -ROOM_D * 0.5f + margin, ROOM_D * 0.5f - margin);
    eyeY = clampf(eyeY, 0.20f, ROOM_H - 0.20f);

    // bulb flicker factor
    g_flicker = computeFlicker(timeSec);

    glutPostRedisplay();
}

// ---------------- Init / reshape ----------------
void reshape(int w, int h) {
    win_width = (w <= 0 ? 1 : w);
    win_height = (h <= 0 ? 1 : h);
    glViewport(0, 0, win_width, win_height);
    applyProjection();
}

void init() {
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    // Antialiasing & nicer quality
    //glEnable(GL_MULTISAMPLE);
    glEnable(GLUT_MULTISAMPLE);

    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    // Lighting + materials
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat M_spec[4] = { 0.25f,0.25f,0.25f,1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, M_spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Fog (blueish)
    glEnable(GL_FOG);
    GLfloat fogColor[4] = { 0.02f, 0.03f, 0.05f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.06f);
    glHint(GL_FOG_HINT, GL_NICEST);

    // Textures
    texFloor = loadTextureSOIL("textures/floor.jpg", 1);
    texWall = loadTextureSOIL("textures/wall.jpg", 1);
    texCeil = loadTextureSOIL("textures/ceiling.jpg", 1);
    texWood = loadTextureSOIL("textures/wood.jpg", 1);
    texPainting = loadTextureSOIL("textures/painting.jpg", 1);
    texEarth = loadTextureSOIL("textures/earth2.jpg", 1);
}

// ---------------- Main ----------------
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA | GLUT_MULTISAMPLE);
    glutInitWindowPosition(win_posx, win_posy);
    glutInitWindowSize(win_width, win_height);
    glutCreateWindow("Room: Smooth FPS + Horror Lighting + SOIL2 Textures");

    // Input callbacks (FreeGLUT)
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(onSpecialDown);
    glutSpecialUpFunc(onSpecialUp);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idle);

    init();
    glutMainLoop();
    return 0;
}
