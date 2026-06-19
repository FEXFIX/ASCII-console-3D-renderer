#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

#define WIDTH     120
#define HEIGHT    40
#define DEF_SCALE 55.0f
#define DEF_DIST  3.0f
#define DRAW_MODE 0

#define MAX_VERTS      200000
#define MAX_FACES      200000
#define MAX_FACE_VERTS 16
#define PI 3.14159265f

typedef struct { float x, y, z; } Vec3;

static char   fb[WIDTH * HEIGHT];
static HANDLE hOut;

static Vec3 verts[MAX_VERTS];   static int vert_count = 0;
static int  faces[MAX_FACES][MAX_FACE_VERTS];
static int  face_sizes[MAX_FACES];   static int face_count = 0;

static int   sx[MAX_VERTS], sy[MAX_VERTS];
static float vz[MAX_VERTS];

static float g_scale_x = DEF_SCALE;
static float g_scale_y = DEF_SCALE * 0.5f;
static float g_dist    = DEF_DIST;

static void fb_clear(void) {
    memset(fb, ' ', WIDTH * HEIGHT);
}

static void fb_set(int x, int y, char c) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    fb[y * WIDTH + x] = c;
}

static void fb_flush(void) {
    DWORD written;
    for (int y = 0; y < HEIGHT; y++) {
        COORD pos = {0, (SHORT)y};
        WriteConsoleOutputCharacterA(hOut, fb + y * WIDTH,
                                     WIDTH, pos, &written);
    }
}

static void draw_line(int x0, int y0, int x1, int y1, char c) {
    int dx =  abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sxs = x0 < x1 ? 1 : -1;
    int sys = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        fb_set(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sxs; }
        if (e2 <= dx) { err += dx; y0 += sys; }
    }
}

//pick symbol by dist/depth
static const char RAMP[] = " .:-=+*#%@";
static char depth_char(float z) {
    float t = (z - (g_dist - 1.5f)) / 3.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int idx = (int)((1.0f - t) * (float)(sizeof(RAMP) - 2));
    return RAMP[idx];
}


static int load_obj(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[512];
    while (fgets(line, sizeof line, fp)) {

        if (line[0] == 'v' && line[1] == ' ') {
            Vec3 v;
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3)
                if (vert_count < MAX_VERTS) verts[vert_count++] = v;
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            int n = 0;
            char *tok = strtok(line + 2, " \t\r\n");
            while (tok && n < MAX_FACE_VERTS) {
                int vi = atoi(tok);
                if (vi < 0) vi = vert_count + vi + 1;
                faces[face_count][n++] = vi - 1;
                tok = strtok(NULL, " \t\r\n");
            }
            if (n >= 2 && face_count < MAX_FACES)
                face_sizes[face_count++] = n;
        }
    }
    fclose(fp);
    return (vert_count > 0 && face_count > 0);
}


static void normalize_model(void) {
    Vec3 mn = verts[0], mx = verts[0];
    for (int i = 1; i < vert_count; i++) {
        if (verts[i].x < mn.x) mn.x = verts[i].x;
        if (verts[i].y < mn.y) mn.y = verts[i].y;
        if (verts[i].z < mn.z) mn.z = verts[i].z;
        if (verts[i].x > mx.x) mx.x = verts[i].x;
        if (verts[i].y > mx.y) mx.y = verts[i].y;
        if (verts[i].z > mx.z) mx.z = verts[i].z;
    }
    Vec3 ctr = { (mn.x+mx.x)*0.5f, (mn.y+mx.y)*0.5f, (mn.z+mx.z)*0.5f };
    float ext = mx.x - mn.x;
    if (mx.y - mn.y > ext) ext = mx.y - mn.y;
    if (mx.z - mn.z > ext) ext = mx.z - mn.z;
    if (ext == 0.0f) ext = 1.0f;
    float s = 2.0f / ext;

    for (int i = 0; i < vert_count; i++) {
        verts[i].x = (verts[i].x - ctr.x) * s;
        verts[i].y = (verts[i].y - ctr.y) * s;
        verts[i].z = (verts[i].z - ctr.z) * s;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model.obj> [scale] [dist]\n", argv[0]);
        return 1;
    }
    if (!load_obj(argv[1])) {
        fprintf(stderr, "Error: could not load '%s'\n", argv[1]);
        return 1;
    }
    if (argc > 2) {
        g_scale_x = (float)atof(argv[2]);
        g_scale_y = g_scale_x * 0.5f;
    }
    if (argc > 3) {
        g_dist = (float)atof(argv[3]);
    }

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    
    if (argc > 4) {
        WORD color = FOREGROUND_INTENSITY;
        if      (*argv[4] == 'r') color |= FOREGROUND_RED;
        else if (*argv[4] == 'g') color |= FOREGROUND_GREEN;
        else if (*argv[4] == 'b') color |= FOREGROUND_BLUE;

        DWORD written;
        COORD origin = {0, 0};
        FillConsoleOutputAttribute(hOut, color, WIDTH * HEIGHT, origin, &written);
    }

    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hOut, &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &ci);
    normalize_model();

    
    float ay = 0.0f;
    float ax = 0.0f;

    while (1) {
        ay += 0.030f;
        ax += 0.013f;

        float cy = cosf(ay), syr = sinf(ay);
        float cx = cosf(ax), sxr = sinf(ax);

        for (int i = 0; i < vert_count; i++) {
            float x = verts[i].x, y = verts[i].y, z = verts[i].z;

            float rx = x * cy - z * syr;
            float rz = x * syr + z * cy;
            float ry = y;

            float ry2 = ry * cx - rz * sxr;
            float rz2 = ry * sxr + rz * cx;
            ry = ry2;
            rz = rz2;

            rz += g_dist;
            if (rz < 0.01f) rz = 0.01f;

            sx[i] = (int)( rx / rz * g_scale_x + WIDTH  / 2);
            sy[i] = (int)(-ry / rz * g_scale_y + HEIGHT / 2);
            vz[i] = rz;
        }

        fb_clear();
        for (int fc = 0; fc < face_count; fc++) {
            int n = face_sizes[fc];
            for (int k = 0; k < n; k++) {
                int a = faces[fc][k];
                int b = faces[fc][(k + 1) % n];
                if (a < 0 || a >= vert_count) continue;
                if (b < 0 || b >= vert_count) continue;


                char c = depth_char((vz[a] + vz[b]) * 0.5f);

                draw_line(sx[a], sy[a], sx[b], sy[b], c);
            }
        }
        fb_flush();

        Sleep(33);
    }
    return 0;
}