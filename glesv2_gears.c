/*
  yagears                  Yet Another Gears OpenGL demo
  Copyright (C) 2013-2015  Nicolas Caramelli

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <GLES2/gl2.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void identity(GLfloat *a)
{
  GLfloat m[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
  };

  memcpy(a, m, sizeof(m));
}

static void multiply(GLfloat *a, const GLfloat *b)
{
  GLfloat m[16];
  GLint i, j;
  div_t d;

  for (i = 0; i < 16; i++) {
    m[i] = 0;
    d = div(i, 4);
    for (j = 0; j < 4; j++)
      m[i] += (a + d.rem)[j * 4] * (b + d.quot * 4)[j];
  }

  memcpy(a, m, sizeof(m));
}

static void translate_x(GLfloat *a, GLfloat transx)
{
  GLfloat m[16] = {
         1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0, 1, 0,
    transx, 0, 0, 1
  };

  multiply(a, m);
}

static void translate_y(GLfloat *a, GLfloat transy)
{
  GLfloat m[16] = {
    1,      0, 0, 0,
    0,      1, 0, 0,
    0,      0, 1, 0,
    0, transy, 0, 1
  };

  multiply(a, m);
}

static void translate_z(GLfloat *a, GLfloat transz)
{
  GLfloat m[16] = {
    1, 0,      0, 0,
    0, 1,      0, 0,
    0, 0,      1, 0,
    0, 0, transz, 1
  };

  multiply(a, m);
}

static void rotate_x(GLfloat *a, GLfloat rotx)
{
  GLfloat s, c;

  sincosf(rotx * M_PI / 180, &s, &c);

  GLfloat m[16] = {
    1,  0, 0, 0,
    0,  c, s, 0,
    0, -s, c, 0,
    0,  0, 0, 1
  };

  multiply(a, m);
}

static void rotate_y(GLfloat *a, GLfloat roty)
{
  GLfloat s, c;

  sincosf(roty * M_PI / 180, &s, &c);

  GLfloat m[16] = {
    c, 0, -s, 0,
    0, 1,  0, 0,
    s, 0,  c, 0,
    0, 0,  0, 1
  };

  multiply(a, m);
}

static void rotate_z(GLfloat *a, GLfloat rotz)
{
  GLfloat s, c;

  sincosf(rotz * M_PI / 180, &s, &c);

  GLfloat m[16] = {
     c, s, 0, 0,
    -s, c, 0, 0,
     0, 0, 1, 0,
     0, 0, 0, 1
  };

  multiply(a, m);
}

static void transpose(GLfloat *a)
{
  GLfloat m[16] = {
    a[0], a[4], a[8],  a[12],
    a[1], a[5], a[9],  a[13],
    a[2], a[6], a[10], a[14],
    a[3], a[7], a[11], a[15]
  };

  memcpy(a, m, sizeof(m));
}

static void invert(GLfloat *a)
{
  GLfloat m[16] = {
         1,      0,      0, 0,
         0,      1,      0, 0,
         0,      0,      1, 0,
    -a[12], -a[13], -a[14], 1,
  };

  a[12] = a[13] = a[14] = 0;
  transpose(a);

  multiply(a, m);
}

/******************************************************************************/

static GLuint program = 0, vertShader = 0, fragShader = 0;
static GLint LightPos_loc, ModelViewProjection_loc, Normal_loc, Color_loc;

struct strip {
  GLint begin;
  GLsizei count;
};

struct gear {
  GLint nvertices;
  GLfloat *vertices;
  GLint nstrips;
  struct strip *strips;
  GLuint vbo;
};

static struct gear *gear1 = NULL, *gear2 = NULL, *gear3 = NULL;

static GLfloat Projection[16], View[16];

static struct gear *create_gear(GLfloat inner, GLfloat outer, GLfloat width, GLint teeth, GLfloat tooth_depth)
{
  struct gear *gear;
  GLfloat r0, r1, r2, da, a1, ai, s[5], c[5];
  GLint i, j;
  GLfloat n[3];
  GLint k = 0;

  gear = calloc(1, sizeof(struct gear));
  if (!gear) {
    printf("calloc gear failed\n");
    return NULL;
  }

  gear->nvertices = 0;
  gear->vertices = calloc(34 * teeth, 6 * sizeof(GLfloat));
  if (!gear->vertices) {
    printf("calloc vertices failed\n");
    return NULL;
  }

  gear->nstrips = 7 * teeth;
  gear->strips = calloc(gear->nstrips, sizeof(struct strip));
  if (!gear->strips) {
    printf("calloc strips failed\n");
    return NULL;
  }

  r0 = inner;
  r1 = outer - tooth_depth / 2;
  r2 = outer + tooth_depth / 2;
  a1 = 2 * M_PI / teeth;
  da = a1 / 4;

  #define normal(nx, ny, nz) \
    n[0] = nx; \
    n[1] = ny; \
    n[2] = nz;

  #define vertex(x, y, z) \
    gear->vertices[6 * gear->nvertices]     = x; \
    gear->vertices[6 * gear->nvertices + 1] = y; \
    gear->vertices[6 * gear->nvertices + 2] = z; \
    gear->vertices[6 * gear->nvertices + 3] = n[0]; \
    gear->vertices[6 * gear->nvertices + 4] = n[1]; \
    gear->vertices[6 * gear->nvertices + 5] = n[2]; \
    gear->nvertices++;

  for (i = 0; i < teeth; i++) {
    ai = i * a1;
    for (j = 0; j < 5; j++) {
      sincosf(ai + j * da, &s[j], &c[j]);
    }

    /* front face begin */
    gear->strips[k].begin = gear->nvertices;
    /* front face normal */
    normal(0, 0, 1);
    /* front face vertices */
    vertex(r2 * c[1], r2 * s[1], width / 2);
    vertex(r2 * c[2], r2 * s[2], width / 2);
    vertex(r1 * c[0], r1 * s[0], width / 2);
    vertex(r1 * c[3], r1 * s[3], width / 2);
    vertex(r0 * c[0], r0 * s[0], width / 2);
    vertex(r1 * c[4], r1 * s[4], width / 2);
    vertex(r0 * c[4], r0 * s[4], width / 2);
    /* front face end */
    gear->strips[k].count = 7;
    k++;

    /* back face begin */
    gear->strips[k].begin = gear->nvertices;
    /* back face normal */
    normal(0, 0, -1);
    /* back face vertices */
    vertex(r2 * c[1], r2 * s[1], -width / 2);
    vertex(r2 * c[2], r2 * s[2], -width / 2);
    vertex(r1 * c[0], r1 * s[0], -width / 2);
    vertex(r1 * c[3], r1 * s[3], -width / 2);
    vertex(r0 * c[0], r0 * s[0], -width / 2);
    vertex(r1 * c[4], r1 * s[4], -width / 2);
    vertex(r0 * c[4], r0 * s[4], -width / 2);
    /* back face end */
    gear->strips[k].count = 7;
    k++;

    /* first outward face begin */
    gear->strips[k].begin = gear->nvertices;
    /* first outward face normal */
    normal(r2 * s[1] - r1 * s[0], r1 * c[0] - r2 * c[1], 0);
    /* first outward face vertices */
    vertex(r1 * c[0], r1 * s[0],  width / 2);
    vertex(r1 * c[0], r1 * s[0], -width / 2);
    vertex(r2 * c[1], r2 * s[1],  width / 2);
    vertex(r2 * c[1], r2 * s[1], -width / 2);
    /* first outward face end */
    gear->strips[k].count = 4;
    k++;

    /* second outward face begin */
    gear->strips[k].begin = gear->nvertices;
    /* second outward face normal */
    normal(s[2] - s[1], c[1] - c[2], 0);
    /* second outward face vertices */
    vertex(r2 * c[1], r2 * s[1],  width / 2);
    vertex(r2 * c[1], r2 * s[1], -width / 2);
    vertex(r2 * c[2], r2 * s[2],  width / 2);
    vertex(r2 * c[2], r2 * s[2], -width / 2);
    /* second outward face end */
    gear->strips[k].count = 4;
    k++;

    /* third outward face begin */
    gear->strips[k].begin = gear->nvertices;
    /* third outward face normal */
    normal(r1 * s[3] - r2 * s[2], r2 * c[2] - r1 * c[3], 0);
    /* third outward face vertices */
    vertex(r2 * c[2], r2 * s[2],  width / 2);
    vertex(r2 * c[2], r2 * s[2], -width / 2);
    vertex(r1 * c[3], r1 * s[3],  width / 2);
    vertex(r1 * c[3], r1 * s[3], -width / 2);
    /* third outward face end */
    gear->strips[k].count = 4;
    k++;

    /* fourth outward face begin */
    gear->strips[k].begin = gear->nvertices;
    /* fourth outward face normal */
    normal(s[4] - s[3], c[3] - c[4], 0);
    /* fourth outward face vertices */
    vertex(r1 * c[3], r1 * s[3],  width / 2);
    vertex(r1 * c[3], r1 * s[3], -width / 2);
    vertex(r1 * c[4], r1 * s[4],  width / 2);
    vertex(r1 * c[4], r1 * s[4], -width / 2);
    /* fourth outward face end */
    gear->strips[k].count = 4;
    k++;

    /* inside face begin */
    gear->strips[k].begin = gear->nvertices;
    /* inside face normal */
    normal(s[0] - s[4], c[4] - c[0], 0);
    /* inside face vertices */
    vertex(r0 * c[0], r0 * s[0],  width / 2);
    vertex(r0 * c[0], r0 * s[0], -width / 2);
    vertex(r0 * c[4], r0 * s[4],  width / 2);
    vertex(r0 * c[4], r0 * s[4], -width / 2);
    /* inside face end */
    gear->strips[k].count = 4;
    k++;
  }

  glGenBuffers(1, &gear->vbo);
  if (!gear->vbo) {
    printf("glGenBuffers failed\n");
    return NULL;
  }

  glBindBuffer(GL_ARRAY_BUFFER, gear->vbo);

  glBufferData(GL_ARRAY_BUFFER, gear->nvertices * 6 * sizeof(GLfloat), gear->vertices, GL_STATIC_DRAW);

  return gear;
}

static void draw_gear(struct gear *gear, GLfloat model_tx, GLfloat model_ty, GLfloat model_rz, const GLfloat *color)
{
  GLfloat ModelView[16], ModelViewProjection[16];
  GLint k;

  memcpy(ModelView, View, sizeof(ModelView));

  translate_x(ModelView, model_tx);
  translate_y(ModelView, model_ty);
  rotate_z(ModelView, model_rz);

  /* Set u_ModelViewProjectionMatrix */
  memcpy(ModelViewProjection, Projection, sizeof(ModelViewProjection));
  multiply(ModelViewProjection, ModelView);
  glUniformMatrix4fv(ModelViewProjection_loc, 1, GL_FALSE, ModelViewProjection);

  /* Set u_NormalMatrix */
  invert(ModelView);
  transpose(ModelView);
  glUniformMatrix4fv(Normal_loc, 1, GL_FALSE, ModelView);

  /* Set u_Color */
  glUniform4fv(Color_loc, 1, color);

  glBindBuffer(GL_ARRAY_BUFFER, gear->vbo);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), NULL);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  for (k = 0; k < gear->nstrips; k++) {
    glDrawArrays(GL_TRIANGLE_STRIP, gear->strips[k].begin, gear->strips[k].count);
  }

  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(0);
}

static void delete_gear(struct gear *gear)
{
  if (gear->vbo) {
    glDeleteBuffers(1, &gear->vbo);
  }

  if (gear->strips) {
    free(gear->strips);
  }

  if (gear->vertices) {
    free(gear->vertices);
  }

  free(gear);
}

/******************************************************************************/

void glesv2_gears_init(int win_width, int win_height)
{
  const char *vertShaderSrc =
    "attribute vec3 a_Position;\n"
    "attribute vec3 a_Normal;\n"
    "uniform vec4 u_LightPos;\n"
    "uniform mat4 u_ModelViewProjectionMatrix;\n"
    "uniform mat4 u_NormalMatrix;\n"
    "uniform vec4 u_Color;\n"
    "varying vec4 v_Color;\n"
    "void main(void)\n"
    "{\n"
    "  gl_Position = u_ModelViewProjectionMatrix * vec4(a_Position, 1);\n"
    "  v_Color = u_Color * dot(normalize(u_LightPos.xyz), normalize(vec3(u_NormalMatrix * vec4(a_Normal, 1))));\n"
    "}";
  const char *fragShaderSrc =
    "precision mediump float;\n"
    "varying vec4 v_Color;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = v_Color;\n"
    "}\n";
  GLint params;
  GLchar *log;
  const GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
  GLfloat zNear = 5, zFar = 60;

  glEnable(GL_DEPTH_TEST);

  program = glCreateProgram();
  if (!program) {
    printf("glCreateProgram failed\n");
    return;
  }

  /* vertex shader */

  vertShader = glCreateShader(GL_VERTEX_SHADER);
  if (!vertShader) {
    printf("glCreateShader vertex failed\n");
    return;
  }

  glShaderSource(vertShader, 1, &vertShaderSrc, NULL);

  glCompileShader(vertShader);
  glGetShaderiv(vertShader, GL_COMPILE_STATUS, &params);
  if (!params) {
    glGetShaderiv(vertShader, GL_INFO_LOG_LENGTH, &params);
    log = calloc(1, params);
    if (!log) {
      printf("calloc log failed\n");
      return;
    }
    glGetShaderInfoLog(vertShader, params, NULL, log);
    printf("glCompileShader vertex failed: %s", log);
    free(log);
    return;
  }

  glAttachShader(program, vertShader);

  /* fragment shader */

  fragShader = glCreateShader(GL_FRAGMENT_SHADER);
  if (!fragShader) {
    printf("glCreateShader fragment failed\n");
    return;
  }

  glShaderSource(fragShader, 1, &fragShaderSrc, NULL);

  glCompileShader(fragShader);
  glGetShaderiv(fragShader, GL_COMPILE_STATUS, &params);
  if (!params) {
    glGetShaderiv(fragShader, GL_INFO_LOG_LENGTH, &params);
    log = calloc(1, params);
    if (!log) {
      printf("calloc log failed\n");
      return;
    }
    glGetShaderInfoLog(fragShader, params, NULL, log);
    printf("glCompileShader fragment failed: %s", log);
    free(log);
    return;
  }

  glAttachShader(program, fragShader);

  /* link and use program */

  glBindAttribLocation(program, 0, "a_Position");
  glBindAttribLocation(program, 1, "a_Normal");

  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &params);
  if (!params) {
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &params);
    log = calloc(1, params);
    if (!log) {
      printf("calloc log failed\n");
      return;
    }
    glGetProgramInfoLog(program, params, NULL, log);
    printf("glLinkProgram failed: %s", log);
    free(log);
    return;
  }

  glUseProgram(program);

  /* Get location of uniform variables */

  LightPos_loc = glGetUniformLocation(program, "u_LightPos");
  ModelViewProjection_loc = glGetUniformLocation(program, "u_ModelViewProjectionMatrix");
  Normal_loc = glGetUniformLocation(program, "u_NormalMatrix");
  Color_loc = glGetUniformLocation(program, "u_Color");

  /* Set u_LightPos */
  glUniform4fv(LightPos_loc, 1, pos);

  gear1 = create_gear(1.0, 4.0, 1.0, 20, 0.7);
  if (!gear1) {
    return;
  }

  gear2 = create_gear(0.5, 2.0, 2.0, 10, 0.7);
  if (!gear2) {
    return;
  }

  gear3 = create_gear(1.3, 2.0, 0.5, 10, 0.7);
  if (!gear3) {
    return;
  }

  memset(Projection, 0, sizeof(Projection));
  Projection[0] = zNear;
  Projection[5] = (GLfloat)win_width/win_height * zNear;
  Projection[10] = -(zFar + zNear) / (zFar - zNear);
  Projection[11] = -1;
  Projection[14] = -2 * zFar * zNear / (zFar - zNear);
}

void glesv2_gears_draw(float view_tz, float view_rx, float view_ry, float model_rz)
{
  const GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
  const GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
  const GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

  if (!gear1 || !gear2 || !gear3) {
    return;
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  identity(View);
  translate_z(View, (GLfloat)view_tz);
  rotate_x(View, (GLfloat)view_rx);
  rotate_y(View, (GLfloat)view_ry);

  draw_gear(gear1, -3.0, -2.0,      (GLfloat)model_rz     , red);
  draw_gear(gear2,  3.1, -2.0, -2 * (GLfloat)model_rz - 9 , green);
  draw_gear(gear3, -3.1,  4.2, -2 * (GLfloat)model_rz - 25, blue);
}

void glesv2_gears_exit()
{
  if (gear1) {
    delete_gear(gear1);
  }

  if (gear2) {
    delete_gear(gear2);
  }

  if (gear3) {
    delete_gear(gear3);
  }

  if (fragShader) {
    glDeleteShader(fragShader);
  }

  if (vertShader) {
    glDeleteShader(vertShader);
  }

  if (program) {
    glDeleteProgram(program);
  }

  printf("%s\n", glGetString(GL_VERSION));
  printf("%s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
}