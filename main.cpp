// Room with FPS camera, animation, lighting, and SOIL2 textures.
// Meets rubric: algebraic (sphere/torus), topological (nested bulb+ring),
// morphological (curves+planes), combinatorial (chairs), transformations,
// animation (bulb sway + rotating Earth), camera+projections, texture mapping.

#include <math.h>
#include <glut.h>
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

// ---------------- Camera (FPS-style) ----------------
float eyeX = 3.0f, eyeY = 1.2f, eyeZ = 3.5f; // start inside room
float yawDeg = -135.0f;  // left/right (around world Y)
float pitchDeg = -8.0f;  // up/down (clamped)
float baseSpeed = 0.08f;   // movement step
float lookStep = 2.0f;    // arrow-key look step (deg)

// computed every frame (forward/right/up unit vectors)
float fwdX = 0, fwdY = 0, fwdZ = -1;
float rgtX = 1, rgtY = 0, rgtZ = 0;
float upX = 0, upY = 1, upZ = 0;

// ---------------- Animation ----------------
int   animate_on = 1;
float timeSec = 0.0f;
int   lastTimeMS = 0;
float earthAngle = 0.0f; // rotation of the textured Earth

// ---------------- Toggles ----------------
int showAxes = 0;

// ---------------- Textures (SOIL2) ----------------
GLuint texFloor = 0, texWall = 0, texWood = 0, texPainting = 0, texEarth = 0;

// ---------------- Utilities ----------------
#define DEG2RAD 0.017453292519943295769f
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float len3(float x, float y, float z) { return sqrtf(x * x + y * y + z * z); }
static void  norm3(float* x, float* y, float* z) { float L = len3(*x, *y, *z); if (L > 0) { *x /= L; *y /= L; *z /= L; } }
static void cross3(float ax, float ay, float az, float bx, float by, float bz, float* rx, float* ry, float* rz) {
    *rx = ay * bz - az * by; *ry = az * bx - ax * bz; *rz = ax * by - ay * bx;
}

// ---------------- Texture loader (SOIL2) ----------------
static GLuint loadTextureSOIL(const char* file, int invertY) {
    int flags = SOIL_FLAG_MIPMAPS | (invertY ? SOIL_FLAG_INVERT_Y : 0);
    GLuint id = SOIL_load_OGL_texture(file, SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, flags);
    if (!id) {
        printf("SOIL2: failed to load '%s' : %s\n", file, SOIL_last_result());
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, id);
    // Filtering + wrapping (repeat tiling)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Modulate with lighting
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

// ---------------- Axes (toggle) ----------------
void axes() {
    if (!showAxes) return;
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    // X (red)
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(2, 0, 0);
    // Y (green)
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, 2, 0);
    // Z (blue)
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, 2);
    glEnd();
    glEnable(GL_LIGHTING);
}

// ---------------- Helpers ----------------
// Untextured cube via GLUT (for quick solids)
static void drawBox(float sx, float sy, float sz) {
    glPushMatrix(); glScalef(sx, sy, sz); glutSolidCube(1.0f); glPopMatrix();
}
// Textured box (manual faces with texture coords)
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
    // +Y (top)
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0);          glVertex3f(-hx, hy, -hz);
    glTexCoord2f(tileU, 0);      glVertex3f(hx, hy, -hz);
    glTexCoord2f(tileU, tileV);  glVertex3f(hx, hy, hz);
    glTexCoord2f(0, tileV);   glVertex3f(-hx, hy, hz);
    // -Y (bottom)
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

// ---------------- Room (SOIL2 textures) ----------------
void drawRoom() {
    const float x0 = -ROOM_W * 0.5f, x1 = ROOM_W * 0.5f;
    const float z0 = -ROOM_D * 0.5f, z1 = ROOM_D * 0.5f;
    const float y0 = 0.0f, y1 = ROOM_H;

    glDisable(GL_CULL_FACE);

    // Floor (textured)
    if (texFloor) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texFloor); }
    glColor3f(1, 1, 1);
    float tile = 8.0f; // repeat texture
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    if (texFloor) glTexCoord2f(0, 0);       glVertex3f(x0, y0, z0);
    if (texFloor) glTexCoord2f(tile, 0);    glVertex3f(x1, y0, z0);
    if (texFloor) glTexCoord2f(tile, tile); glVertex3f(x1, y0, z1);
    if (texFloor) glTexCoord2f(0, tile);    glVertex3f(x0, y0, z1);
    glEnd();
    if (texFloor) { glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D); }

    // Ceiling (plain)
    glColor3f(0.88f, 0.88f, 0.92f);
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    glEnd();

    // Walls (textured)
    if (texWall) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texWall); }
    glColor3f(1, 1, 1);
    float wallU = 4.0f, wallV = 2.0f;
    glBegin(GL_QUADS);
    // +X wall
    glNormal3f(-1, 0, 0);
    if (texWall) glTexCoord2f(0, 0);          glVertex3f(x1, y0, z0);
    if (texWall) glTexCoord2f(wallU, 0);      glVertex3f(x1, y0, z1);
    if (texWall) glTexCoord2f(wallU, wallV);  glVertex3f(x1, y1, z1);
    if (texWall) glTexCoord2f(0, wallV);      glVertex3f(x1, y1, z0);
    // -X wall
    glNormal3f(1, 0, 0);
    if (texWall) glTexCoord2f(0, 0);          glVertex3f(x0, y0, z1);
    if (texWall) glTexCoord2f(wallU, 0);      glVertex3f(x0, y0, z0);
    if (texWall) glTexCoord2f(wallU, wallV);  glVertex3f(x0, y1, z0);
    if (texWall) glTexCoord2f(0, wallV);      glVertex3f(x0, y1, z1);
    // +Z wall
    glNormal3f(0, 0, -1);
    if (texWall) glTexCoord2f(0, 0);          glVertex3f(x0, y0, z1);
    if (texWall) glTexCoord2f(wallU, 0);      glVertex3f(x1, y0, z1);
    if (texWall) glTexCoord2f(wallU, wallV);  glVertex3f(x1, y1, z1);
    if (texWall) glTexCoord2f(0, wallV);      glVertex3f(x0, y1, z1);
    // -Z wall (front)
    glNormal3f(0, 0, 1);
    if (texWall) glTexCoord2f(0, 0);          glVertex3f(x1, y0, z0);
    if (texWall) glTexCoord2f(wallU, 0);      glVertex3f(x0, y0, z0);
    if (texWall) glTexCoord2f(wallU, wallV);  glVertex3f(x0, y1, z0);
    if (texWall) glTexCoord2f(0, wallV);      glVertex3f(x1, y1, z0);
    glEnd();
    if (texWall) { glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D); }

    // A framed painting on the -Z wall (slightly in front to avoid z-fighting)
    if (texPainting) {
        float pw = 1.4f, ph = 0.9f, z = z0 + 0.001f, y = 1.6f;
        glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texPainting);
        glColor3f(1, 1, 1);
        glBegin(GL_QUADS);
        glNormal3f(0, 0, 1);
        glTexCoord2f(0, 0); glVertex3f(-pw * 0.5f, y - ph * 0.5f, z);
        glTexCoord2f(1, 0); glVertex3f(pw * 0.5f, y - ph * 0.5f, z);
        glTexCoord2f(1, 1); glVertex3f(pw * 0.5f, y + ph * 0.5f, z);
        glTexCoord2f(0, 1); glVertex3f(-pw * 0.5f, y + ph * 0.5f, z);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D);

        // simple frame (untextured)
        glColor3f(0.25f, 0.15f, 0.08f);
        float t = 0.03f;
        glBegin(GL_QUADS);
        // bottom
        glVertex3f(-pw * 0.5f - t, y - ph * 0.5f - t, z); glVertex3f(pw * 0.5f + t, y - ph * 0.5f - t, z);
        glVertex3f(pw * 0.5f + t, y - ph * 0.5f, z); glVertex3f(-pw * 0.5f - t, y - ph * 0.5f, z);
        // top
        glVertex3f(-pw * 0.5f - t, y + ph * 0.5f, z); glVertex3f(pw * 0.5f + t, y + ph * 0.5f, z);
        glVertex3f(pw * 0.5f + t, y + ph * 0.5f + t, z); glVertex3f(-pw * 0.5f - t, y + ph * 0.5f + t, z);
        // left
        glVertex3f(-pw * 0.5f - t, y - ph * 0.5f, z); glVertex3f(-pw * 0.5f, y - ph * 0.5f, z);
        glVertex3f(-pw * 0.5f, y + ph * 0.5f, z); glVertex3f(-pw * 0.5f - t, y + ph * 0.5f, z);
        // right
        glVertex3f(pw * 0.5f, y - ph * 0.5f, z); glVertex3f(pw * 0.5f + t, y - ph * 0.5f, z);
        glVertex3f(pw * 0.5f + t, y + ph * 0.5f, z); glVertex3f(pw * 0.5f, y + ph * 0.5f, z);
        glEnd();
    }
}

// ---------------- Furniture ----------------
void drawTable() {
    const float topW = 1.20f, topD = 0.80f, topT = 0.08f;
    const float height = 0.75f;
    const float legT = 0.08f;
    const float legH = height - topT * 0.5f;

    // Top (textured wood)
    if (texWood) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texWood); glColor3f(1, 1, 1); }
    else glColor3f(0.55f, 0.34f, 0.20f);
    glPushMatrix();
    glTranslatef(0.0f, height, 0.0f);
    if (texWood) { drawTexturedBox(topW, topT, topD, 1.5f, 1.0f); }
    else { drawBox(topW, topT, topD); }
    glPopMatrix();
    if (texWood) { glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D); }

    // Legs (untextured)
    glColor3f(0.48f, 0.29f, 0.16f);
    const float halfW = topW * 0.5f - legT * 0.5f;
    const float halfD = topD * 0.5f - legT * 0.5f;
    const float y = legH * 0.5f;
    glPushMatrix(); glTranslatef(halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
}

void drawChair() {
    const float seatW = 0.45f, seatD = 0.45f, seatT = 0.06f;
    const float seatH = 0.45f;
    const float legT = 0.06f;
    const float backH = 0.45f;

    glColor3f(0.60f, 0.36f, 0.22f);
    glPushMatrix(); glTranslatef(0.0f, seatH, 0.0f); drawBox(seatW, seatT, seatD); glPopMatrix();

    glColor3f(0.50f, 0.30f, 0.18f);
    const float legH = seatH - seatT * 0.5f;
    const float halfW = seatW * 0.5f - legT * 0.5f;
    const float halfD = seatD * 0.5f - legT * 0.5f;
    const float y = legH * 0.5f;
    glPushMatrix(); glTranslatef(halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();

    glColor3f(0.58f, 0.34f, 0.20f);
    glPushMatrix(); glTranslatef(0.0f, seatH + backH * 0.5f, -seatD * 0.5f + legT * 0.5f);
    drawBox(seatW, backH, legT); glPopMatrix();
}

// Earth: textured GLU sphere (algebraic) rotating on the table
void drawTexturedEarth(float radius) {
    if (!texEarth) return; // no texture, skip
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texEarth);

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);

    glColor3f(1, 1, 1);
    glPushMatrix();
    glRotatef(earthAngle, 0, 1, 0);   // rotate over time
    // Optional: align poles if your map needs it
    // glRotatef(-90, 1,0,0);
    gluSphere(quad, radius, 48, 48);
    glPopMatrix();

    gluDeleteQuadric(quad);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

// ---------------- Bulb + lampshade (algebraic torus, nested topology) ----------------
void drawBulbLampAndLight() {
    const float anchorY = ROOM_H - 0.05f;
    const float cordLen = 0.28f;

    // Sway angle over time
    float sway = animate_on ? 10.0f * sinf(timeSec * 1.4f) : 0.0f;

    // Cord
    glColor3f(0.2f, 0.2f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, anchorY, 0.0f);
    glRotatef(sway, 0.0f, 0.0f, 1.0f);         // pendulum swing about Z-axis
    glTranslatef(0.0f, -cordLen * 0.5f, 0.0f);
    drawBox(0.02f, cordLen, 0.02f);
    glPopMatrix();

    // Bulb (emissive) + point light colocated
    GLfloat emit[4] = { 1.0f, 0.96f, 0.85f, 1.0f };
    GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glPushMatrix();
    glTranslatef(0.0f, anchorY, 0.0f);
    glRotatef(sway, 0.0f, 0.0f, 1.0f);
    glTranslatef(0.0f, -cordLen, 0.0f);        // end of cord (bulb position)

    // Light0 position in current modelview (at bulb)
    GLfloat Lpos[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, Lpos);

    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emit);
    glColor3f(1.0f, 1.0f, 0.85f);
    glutSolidSphere(0.08f, 24, 24);            // algebraic sphere (parametric)
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, zero);

    // Lampshade: algebraic torus (nested around bulb)
    glColor3f(0.85f, 0.82f, 0.78f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);        // orient torus horizontally
    glutSolidTorus(0.025f, 0.16f, 24, 48);     // (tube radius, ring radius)
    glPopMatrix();
}

// ---------------- Camera math ----------------
void updateCameraBasis() {
    // Compute forward from yaw/pitch (OpenGL right-handed, -Z forward)
    float yawR = yawDeg * DEG2RAD;
    float pitR = pitchDeg * DEG2RAD;

    fwdX = cosf(pitR) * sinf(yawR);
    fwdY = sinf(pitR);
    fwdZ = -cosf(pitR) * cosf(yawR);
    norm3(&fwdX, &fwdY, &fwdZ);

    // Right = normalize(cross(forward, worldUp))
    float rx, ry, rz;
    cross3(fwdX, fwdY, fwdZ, 0.0f, 1.0f, 0.0f, &rx, &ry, &rz);
    rgtX = rx; rgtY = ry; rgtZ = rz; norm3(&rgtX, &rgtY, &rgtZ);

    // Up = cross(right, forward)
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

// ---------------- Input ----------------
void keyboard(unsigned char key, int x, int y) {
    int mod = glutGetModifiers();
    float speed = baseSpeed * ((mod & GLUT_ACTIVE_SHIFT) ? 3.0f : 1.0f);

    updateCameraBasis();
    switch (key) {
    case 'w': eyeX += fwdX * speed; eyeY += fwdY * speed; eyeZ += fwdZ * speed; break;
    case 's': eyeX -= fwdX * speed; eyeY -= fwdY * speed; eyeZ -= fwdZ * speed; break;
    case 'a': eyeX -= rgtX * speed; eyeY -= rgtY * speed; eyeZ -= rgtZ * speed; break;
    case 'd': eyeX += rgtX * speed; eyeY += rgtY * speed; eyeZ += rgtZ * speed; break;
    case 'q': eyeY += speed; break;
    case 'e': eyeY -= speed; break;

    case 'p': use_perspective = !use_perspective; applyProjection(); break;
    case 'z': if (use_perspective) { fovy = clampf(fovy - 2.0f, 20.0f, 90.0f); }
            else { ortho_scale = clampf(ortho_scale * 0.9f, 1.0f, 10.0f); } applyProjection(); break;
    case 'x': if (use_perspective) { fovy = clampf(fovy + 2.0f, 20.0f, 90.0f); }
            else { ortho_scale = clampf(ortho_scale / 0.9f, 1.0f, 10.0f); } applyProjection(); break;

    case 'm': animate_on = !animate_on; break;
    case 't': showAxes = !showAxes; break;

    case 'r': // reset camera
        eyeX = 3.0f; eyeY = 1.2f; eyeZ = 3.5f; yawDeg = -135.0f; pitchDeg = -8.0f;
        fovy = 60.0f; ortho_scale = 3.5f; use_perspective = 1; applyProjection();
        break;

    case 27: exit(0); // ESC
    }
    // Keep camera inside room bounds (optional clamp)
    float margin = 0.25f;
    float xHalf = ROOM_W * 0.5f - margin;
    float zHalf = ROOM_D * 0.5f - margin;
    eyeX = clampf(eyeX, -xHalf, xHalf);
    eyeZ = clampf(eyeZ, -zHalf, zHalf);
    eyeY = clampf(eyeY, 0.20f, ROOM_H - 0.20f);

    glutPostRedisplay();
}

void keyboardSpecial(int key, int x, int y) {
    switch (key) {
    case GLUT_KEY_LEFT:  yawDeg -= lookStep; break;
    case GLUT_KEY_RIGHT: yawDeg += lookStep; break;
    case GLUT_KEY_UP:    pitchDeg += lookStep; break;
    case GLUT_KEY_DOWN:  pitchDeg -= lookStep; break;
    }
    pitchDeg = clampf(pitchDeg, -89.0f, 89.0f);
    glutPostRedisplay();
}

// ---------------- Display & idle ----------------
void placeChairsAroundTable() {
    const float tHalfW = 1.20f * 0.5f; // 0.60
    const float tHalfD = 0.80f * 0.5f; // 0.40
    const float seatHalf = 0.45f * 0.5f;
    const float gap = 0.25f;

    // Front (-Z) faces +Z
    glPushMatrix(); glTranslatef(0.0f, 0.0f, -(tHalfD + gap + seatHalf));                    drawChair(); glPopMatrix();
    // Back (+Z) face -Z
    glPushMatrix(); glTranslatef(0.0f, 0.0f, (tHalfD + gap + seatHalf)); glRotatef(180, 0, 1, 0); drawChair(); glPopMatrix();
    // Left (-X) face +X
    glPushMatrix(); glTranslatef(-(tHalfW + gap + seatHalf), 0.0f, 0.0f); glRotatef(90, 0, 1, 0); drawChair(); glPopMatrix();
    // Right (+X) face -X
    glPushMatrix(); glTranslatef((tHalfW + gap + seatHalf), 0.0f, 0.0f); glRotatef(-90, 0, 1, 0); drawChair(); glPopMatrix();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Camera
    updateCameraBasis();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eyeX, eyeY, eyeZ, eyeX + fwdX, eyeY + fwdY, eyeZ + fwdZ, upX, upY, upZ);

    // Lighting parameters (Light0 positioned with bulb later)
    GLfloat L_amb[4] = { 0.10f, 0.10f, 0.10f, 1.0f };
    GLfloat L_dif[4] = { 1.00f, 1.00f, 0.95f, 1.0f };
    GLfloat L_spe[4] = { 1.00f, 1.00f, 1.00f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, L_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, L_dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, L_spe);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.02f);

    // Scene
    drawRoom();
    axes();

    // Table + chairs
    drawTable();
    placeChairsAroundTable();

    // Earth on table (textured sphere)
    glPushMatrix();
    glTranslatef(0.35f, 0.90f, 0.05f); // placed above table
    drawTexturedEarth(0.18f);
    glPopMatrix();

    // Bulb (sphere) + nested torus + dynamic light position
    drawBulbLampAndLight();

    // 2D overlay
    displayLabel();

    glutSwapBuffers();
}

void idle() {
    int t = glutGet(GLUT_ELAPSED_TIME); // ms since program start
    if (lastTimeMS == 0) lastTimeMS = t;
    int dt = t - lastTimeMS;
    lastTimeMS = t;
    if (animate_on) {
        timeSec += dt * 0.001f;   // seconds
        earthAngle += 10.0f * (dt * 0.001f); // 10 deg/sec
        if (earthAngle >= 360.0f) earthAngle -= 360.0f;
    }
    glutPostRedisplay();
}

// ---------------- Init / reshape ----------------
void reshape(int w, int h) {
    win_width = (w <= 0 ? 1 : w); win_height = (h <= 0 ? 1 : h);
    glViewport(0, 0, win_width, win_height);
    applyProjection();
}

void init() {
    glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    // Lighting + materials
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat M_spec[4] = { 0.25f,0.25f,0.25f,1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, M_spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // ---- SOIL2: load textures (adjust file names as you like) ----
    texFloor = loadTextureSOIL("textures/floor.jpg", 1);
    texWall = loadTextureSOIL("textures/wall.jpg", 1);
    texWood = loadTextureSOIL("textures/wood.jpg", 1);
    texPainting = loadTextureSOIL("textures/painting.jpg", 1);
    texEarth = loadTextureSOIL("textures/painting.jpg", 1);
}

// ---------------- Main ----------------
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);
    glutInitWindowPosition(win_posx, win_posy);
    glutInitWindowSize(win_width, win_height);
    glutCreateWindow("Room: FPS Camera + Animation + SOIL2 Textures");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(keyboardSpecial);
    glutIdleFunc(idle);

    init();
    glutMainLoop();
    return 0;
}
