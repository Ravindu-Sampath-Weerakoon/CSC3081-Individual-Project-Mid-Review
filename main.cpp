

#include <math.h>
#include <glut.h>

// ---------------- Window / camera ----------------
int win_posx = 100, win_posy = 100;
int win_width = 720, win_height = 480;

float ex = 3.0f, ey = 1.5f, ez = 3.0f;  // eye position
float cx = 0.0f, cy = 0.8f, cz = 0.0f;  // look-at target
float upx = 0.0f, upy = 1.0f, upz = 0.0f;
float fovy = 60.0f, z_near = 0.1f, z_far = 100.0f;

float cam_lx = 0.0f, cam_ly = 0.0f, cam_lz = 0.0f; // (unused but kept)
float cam_posx = 0.0f, cam_posy = 0.0f, cam_posz = 0.0f;
float cam_rotx = 0.0f, cam_roty = 0.0f, cam_rotz = 0.0f;

float move_step = 0.1f;

// ---------------- Text overlay ----------------
void renderBitmapString(float x, float y, void* font, const char* string) {
    glRasterPos2f(x, y);
    while (*string) {
        glutBitmapCharacter(font, *string);
        string++;
    }
}

void displayLabel() {
    // Save current projection/modelview
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, win_width, 0, win_height);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glColor3f(1.0f, 1.0f, 0.8f);
    void* font = GLUT_BITMAP_8_BY_13;
    float x = 10.0f;
    float y = win_height - 20.0f;
    int line_height = 18;

    renderBitmapString(x, y, font, "W/S: Move Forward / Backward");
    renderBitmapString(x, y -= line_height, font, "A/D: Move Left / Right");
    renderBitmapString(x, y -= line_height, font, "Q/E: Move Up / Down");
    renderBitmapString(x, y -= line_height, font, "Arrow Keys: Rotate X / Y");
    renderBitmapString(x, y -= line_height, font, "Shift + Left/Right: Rotate Z");
    renderBitmapString(x, y -= line_height, font, "ESC: Exit");

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);

    // Restore matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ---------------- Helpers ----------------
static void drawBox(float sx, float sy, float sz) {
    glPushMatrix();
    glScalef(sx, sy, sz);
    glutSolidCube(1.0f);
    glPopMatrix();
}

// ---------------- Geometry ----------------
// Room dimensions (centered at origin in XZ; floor at y=0)
const float ROOM_W = 8.0f;  // X dimension
const float ROOM_D = 8.0f;  // Z dimension
const float ROOM_H = 3.0f;  // Y dimension

void drawRoom() {
    const float x0 = -ROOM_W * 0.5f, x1 = ROOM_W * 0.5f;
    const float z0 = -ROOM_D * 0.5f, z1 = ROOM_D * 0.5f;
    const float y0 = 0.0f, y1 = ROOM_H;

    glDisable(GL_CULL_FACE);

    // Floor
    glColor3f(0.30f, 0.30f, 0.35f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z1);
    glEnd();

    // Ceiling
    glColor3f(0.85f, 0.85f, 0.88f);
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z0);
    glEnd();

    // Walls (light gray)
    glColor3f(0.70f, 0.70f, 0.75f);
    glBegin(GL_QUADS);
    // +X wall (right)
    glNormal3f(-1, 0, 0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y0, z1);
    // -X wall (left)
    glNormal3f(1, 0, 0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z0);
    glVertex3f(x0, y0, z0);
    // +Z wall (back)
    glNormal3f(0, 0, -1);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);
    // -Z wall (front)
    glNormal3f(0, 0, 1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    glEnd();
}

// Table centered at origin
void drawTable() {
    const float topW = 1.20f, topD = 0.80f, topT = 0.08f;
    const float height = 0.75f;
    const float legT = 0.08f;
    const float legH = height - topT * 0.5f;

    // Table top
    glColor3f(0.55f, 0.33f, 0.20f);
    glPushMatrix();
    glTranslatef(0.0f, height, 0.0f);
    drawBox(topW, topT, topD);
    glPopMatrix();

    // Legs (four corners)
    glColor3f(0.50f, 0.30f, 0.18f);
    const float halfW = topW * 0.5f - legT * 0.5f;
    const float halfD = topD * 0.5f - legT * 0.5f;
    const float y = legH * 0.5f;

    glPushMatrix(); glTranslatef(halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
}

// A simple wooden chair facing +Z by default (backrest at -Z)
void drawChair() {
    const float seatW = 0.45f, seatD = 0.45f, seatT = 0.06f;
    const float seatH = 0.45f;
    const float legT = 0.06f;
    const float backH = 0.45f;

    // Seat
    glColor3f(0.60f, 0.36f, 0.22f);
    glPushMatrix();
    glTranslatef(0.0f, seatH, 0.0f);
    drawBox(seatW, seatT, seatD);
    glPopMatrix();

    // Legs
    glColor3f(0.50f, 0.30f, 0.18f);
    const float legH = seatH - seatT * 0.5f;
    const float halfW = seatW * 0.5f - legT * 0.5f;
    const float halfD = seatD * 0.5f - legT * 0.5f;
    const float y = legH * 0.5f;

    glPushMatrix(); glTranslatef(halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(-halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();
    glPushMatrix(); glTranslatef(halfW, y, -halfD); drawBox(legT, legH, legT); glPopMatrix();

    // Backrest (single panel)
    glColor3f(0.58f, 0.34f, 0.20f);
    glPushMatrix();
    glTranslatef(0.0f, seatH + backH * 0.5f, -seatD * 0.5f + legT * 0.5f);
    drawBox(seatW, backH, legT);
    glPopMatrix();
}

// Central bulb (emissive sphere) + small cord
void drawBulbAndCord() {
    const float bulbY = ROOM_H - 0.30f;  // a bit below ceiling
    const float cordLen = 0.25f;

    // Cord
    glColor3f(0.20f, 0.20f, 0.20f);
    glPushMatrix();
    glTranslatef(0.0f, ROOM_H - cordLen * 0.5f, 0.0f);
    drawBox(0.02f, cordLen, 0.02f);
    glPopMatrix();

    // Bulb (emissive)
    GLfloat emit[4] = { 1.0f, 0.95f, 0.85f, 1.0f };
    GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    glPushMatrix();
    glTranslatef(0.0f, bulbY, 0.0f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emit);
    glColor3f(1.0f, 1.0f, 0.85f);
    glutSolidSphere(0.08f, 24, 24);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, zero);
    glPopMatrix();
}

// ---------------- Camera & input ----------------
void camera() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(ex + cam_lx, ey + cam_ly, ez + cam_lz,  // eye
        cx, cy, cz,           // center
        upx, upy, upz);         // up

    // World-space transforms to simulate camera motion/orbiting
    glTranslatef(cam_posx, cam_posy, cam_posz);
    glRotatef(cam_rotx, 1.0f, 0.0f, 0.0f);
    glRotatef(cam_roty, 0.0f, 1.0f, 0.0f);
    glRotatef(cam_rotz, 0.0f, 0.0f, 1.0f);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'w': cam_posz -= move_step; break;
    case 's': cam_posz += move_step; break;
    case 'a': cam_posx -= move_step; break;
    case 'd': cam_posx += move_step; break;
    case 'q': cam_posy += move_step; break;
    case 'e': cam_posy -= move_step; break;
    case 27:  exit(0);               break; // ESC
    }
    glutPostRedisplay();
}

void keyboardSpecial(int key, int x, int y) {
    int multiplier = 10;
    int mod = glutGetModifiers();

    switch (key) {
    case GLUT_KEY_LEFT:
        if (mod == GLUT_ACTIVE_SHIFT) cam_rotz -= move_step * multiplier;
        else                          cam_roty -= move_step * multiplier;
        break;
    case GLUT_KEY_RIGHT:
        if (mod == GLUT_ACTIVE_SHIFT) cam_rotz += move_step * multiplier;
        else                          cam_roty += move_step * multiplier;
        break;
    case GLUT_KEY_UP:
        cam_rotx += move_step * multiplier;
        break;
    case GLUT_KEY_DOWN:
        cam_rotx -= move_step * multiplier;
        break;
    }
    glutPostRedisplay();
}

// ---------------- Display & GL setup ----------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera();

    // Point light at bulb position (world coords)
    GLfloat lightPos[4] = { 0.0f, ROOM_H - 0.30f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Scene
    drawRoom();
    drawTable();

    // Chairs around the table
    // Table half sizes:
    const float tHalfW = 1.20f * 0.5f; // 0.60
    const float tHalfD = 0.80f * 0.5f; // 0.40
    const float seatHalf = 0.45f * 0.5f; // 0.225
    const float gap = 0.25f;

    // Front (negative Z): faces +Z (no rotation)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -(tHalfD + gap + seatHalf)); // ~ -0.875
    drawChair();
    glPopMatrix();

    // Back (positive Z): face -Z (rotate 180)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, (tHalfD + gap + seatHalf)); // ~ +0.875
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    drawChair();
    glPopMatrix();

    // Left (negative X): face +X (rotate +90)
    glPushMatrix();
    glTranslatef(-(tHalfW + gap + seatHalf), 0.0f, 0.0f); // ~ -1.075
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    drawChair();
    glPopMatrix();

    // Right (positive X): face -X (rotate -90)
    glPushMatrix();
    glTranslatef((tHalfW + gap + seatHalf), 0.0f, 0.0f); // ~ +1.075
    glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
    drawChair();
    glPopMatrix();

    // Bulb geometry (emissive) + cord
    drawBulbAndCord();

    // 2D overlay
    displayLabel();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    win_width = (w <= 0) ? 1 : w;
    win_height = (h <= 0) ? 1 : h;

    glViewport(0, 0, win_width, win_height);

    GLfloat aspect_ratio = (win_height == 0) ? 1.0f
        : (GLfloat)win_width / (GLfloat)win_height;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fovy, aspect_ratio, z_near, z_far);

    glMatrixMode(GL_MODELVIEW);
}

void init() {
    glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);       // correct normals after scaling

    // Basic lighting (point light set each frame in display)
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat L_amb[4] = { 0.12f, 0.12f, 0.12f, 1.0f };
    GLfloat L_dif[4] = { 1.00f, 1.00f, 0.95f, 1.0f };
    GLfloat L_spe[4] = { 1.00f, 1.00f, 1.00f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, L_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, L_dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, L_spe);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.05f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.02f);

    // Material defaults (specular once; ambient+diffuse from glColor)
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat M_spec[4] = { 0.25f, 0.25f, 0.25f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, M_spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
}

// ---------------- Main ----------------
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);

    glutInitWindowPosition(win_posx, win_posy);
    glutInitWindowSize(win_width, win_height);
    glutCreateWindow("Room with Bulb, Table, Chairs (GLUT)");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutSpecialFunc(keyboardSpecial);
    glutKeyboardFunc(keyboard);

    init();
    glutMainLoop();
    return 0;
}
