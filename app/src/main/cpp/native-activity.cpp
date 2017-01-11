#include <jni.h>
#include <errno.h>
#include <math.h>

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "include/Emulator.h"

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))
#define AssertGL(x) { x; GLenum __gle = glGetError(); LOGW(__gle == GL_NO_ERROR ? "ALL OK" : "FUCK %d", __gle); }

void update_texture(struct engine *engine);

void init_emulator(engine *pEngine);

struct engine {
    struct android_app *app;
    Emulator *emu;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
    int time;
    int32_t touchX;
    int32_t touchY;

    GLuint prog;
    GLuint vertexBuffer;
    GLuint indexBuffer;
    GLint positionSlot;
    GLint texSlot;
    GLint uvSlot;
    GLuint textureID;
};

struct Vertex {
    float posX;
    float posY;
    float uvS;
    float uvT;
};

float Vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 0.0f
};

GLushort Indices[] = {
        0, 1, 3,
        3, 1, 2
};

//короче константы - размер текстуры в пихелях
const GLuint width = 256;
const GLuint height = 240;
const int stride = 4;

//сама текстура Меняешь и потом вызываешь update_texture(двигатель)
//формат RGBA. С RGB пидорасило, так что сорянчик -_-
GLubyte spriteData[width * height * stride] = {255};

const char *shader_vtx =
                "precision mediump float;\n"
                "precision mediump int;\n"
                "attribute vec2 a_pos;\n"
                "attribute vec2 a_uv;\n"
                "varying vec2 v_uv;\n"
                "void main() {\n"
                "v_uv = a_uv;\n"
                "gl_Position.xy = a_pos.xy;"
                "gl_Position.w = 1.0;\n"
                "gl_Position.z = 0.0;\n"
                "}\n";

const char *shader_frg =
                "precision mediump float;\n"
                "varying vec2 v_uv;\n"
                "uniform sampler2D u_s_texture;\n"
                "void main()  {\n"
                "gl_FragColor = texture2D(u_s_texture, v_uv);\n"
                "}\n";


static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}


int init_display(struct engine *engine) {
    const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    EGLint attribList[] =
            {
                    EGL_CONTEXT_CLIENT_VERSION, 2,
                    EGL_NONE
            };

    EGLint w, h, dummy, format;
    EGLint numConfigs;
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display, 0, 0);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    context = eglCreateContext(display, config, NULL, attribList);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGW("Unable to eglMakeCurrent");
        return -1;
    }

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    engine->display = display;
    engine->context = context;
    engine->surface = surface;
    engine->width = w;
    engine->height = h;

    // Initialize GL state.
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, w, h);


    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);
    printGLString("GLSL", GL_SHADER_COMPILER);

    return 0;
}

static void init_shader(struct engine *engine) {
    GLuint program = glCreateProgram();
    GLuint shader_vr = glCreateShader(GL_VERTEX_SHADER);
    GLuint shader_fr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader_vr, 1, (const GLchar **) &shader_vtx, NULL);
    glCompileShader(shader_vr);
    glAttachShader(program, shader_vr);
    glShaderSource(shader_fr, 1, (const GLchar **) &shader_frg, NULL);
    glCompileShader(shader_fr);
    glAttachShader(program, shader_fr);
    glLinkProgram(program);

    engine->positionSlot = glGetAttribLocation(program, "a_pos");
    engine->uvSlot = glGetAttribLocation(program, "a_uv");
    engine->texSlot = glGetUniformLocation(program, "u_s_texture");

    GLint logLength;
    glGetShaderiv(shader_fr, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *) malloc(logLength);
        glGetShaderInfoLog(shader_fr, logLength, &logLength, log);
        LOGI("%s bla bla", log);
        free(log);
    }

    glGetShaderiv(shader_vr, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *) malloc(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, log);
        LOGI("%s bla la", log);
        free(log);
    }

    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
        GLchar *log = (GLchar *) malloc(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, log);
        LOGI("%s bla la", log);
        free(log);
    }

    GLint b;
    glGetProgramiv(program, GL_LINK_STATUS, &b);
    LOGW(b != GL_TRUE ? "SHIIIT1" : "ALL OK");
    glGetShaderiv(shader_fr, GL_LINK_STATUS, &b);
    LOGW(b != GL_TRUE ? "SHIIIT2" : "ALL OK");
    glGetShaderiv(shader_vr, GL_LINK_STATUS, &b);
    LOGW(b != GL_TRUE ? "SHIIIT3" : "ALL OK");

    engine->prog = program;
    return;
}

void update_texture(struct engine *engine) {
    glBindTexture(GL_TEXTURE_2D, engine->textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 spriteData);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void create_buffers_and_texture(struct engine *engine) {
    glGenBuffers(1, &engine->vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, engine->vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(struct Vertex), Vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &engine->indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, engine->indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLushort), Indices, GL_STATIC_DRAW);

    glGenTextures(1, &engine->textureID);

    update_texture(engine);
}

static int val = 0;

void draw_frame(struct engine *engine) {
    if (engine->display == NULL) {
        return;
    }

    //LOGW("draging another frame, meeeh");

    glViewport(0, 0, engine->width, engine->height);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(engine->prog);

    if (engine->positionSlot != -1) {
        glEnableVertexAttribArray(engine->positionSlot);
        glVertexAttribPointer(engine->positionSlot, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex),
                              (const GLvoid *) offsetof(struct Vertex, posX));
    } else {
        LOGW("%s kokoe-to gowno s a_pos");
    }

    if (engine->uvSlot != -1) {
        glEnableVertexAttribArray(engine->uvSlot);
        glVertexAttribPointer(engine->uvSlot, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex),
                              (const GLvoid *) offsetof(struct Vertex, uvS));
    } else {
        LOGW("%s kokoe-to gowno s UV-zaloopoi");
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, engine->textureID);
    glUniform1i(engine->uvSlot, 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    eglSwapBuffers(engine->display, engine->surface);

    glDisableVertexAttribArray(engine->uvSlot);
    glDisableVertexAttribArray(engine->positionSlot);
}

void terminate_display(struct engine *engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

int32_t handle_input(struct android_app *app, AInputEvent *event) {
    struct engine *engine = (struct engine *) app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        engine->touchX = AMotionEvent_getX(event, 0);
        engine->touchY = AMotionEvent_getY(event, 0);
        LOGI("x=%d\ty=%d", engine->touchX, engine->touchY);
        return 1;
    }
    return 0;
}

void handle_cmd(struct android_app *app, int32_t cmd) {
    struct engine *engine = (struct engine *) app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            break;
        case APP_CMD_INIT_WINDOW:
            if (engine->app->window != NULL) {
                init_display(engine);
                create_buffers_and_texture(engine);
                init_shader(engine);
                draw_frame(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            terminate_display(engine);
            break;
        case APP_CMD_LOST_FOCUS:
            draw_frame(engine);
            break;
    }
}

void vsync_call(void *userData) {
    struct engine *engine = (struct engine *)userData;
    update_texture(engine);
    draw_frame(engine);
}

void put_pixel(int x, int y, int color, void *userData) {
    union {
        uint32_t num;
        uint8_t bytes[4];
    };

    //LOGW("%d %d %d", x, y, color);

    num = color;
    spriteData[x*stride + y + 0] = bytes[0];
    spriteData[x*stride + y + 1] = bytes[1];
    spriteData[x*stride + y + 2] = bytes[2];
    spriteData[x*stride + y + 3] = bytes[3];
}

void init_emulator(struct engine *engine) {
    engine->emu = new Emulator("/sdcard/file.nes", put_pixel, vsync_call, (void *)engine);
    engine->emu->preRun();
}

void android_main(struct android_app *state) {
    app_dummy();

    struct engine engine;

    memset(spriteData, 0xff, width*height*stride);

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;
    engine.app = state;

    init_emulator(&engine);

    while (1) {
        int ident;
        int events;
        struct android_poll_source *source;

        while ((ident = ALooper_pollAll(0, NULL, &events, (void **) &source)) >= 0) {

            if (source != NULL) {
                source->process(state, source);
            }

            if (state->destroyRequested != 0) {
                terminate_display(&engine);
                return;
            }
        }

        engine.emu->makeStep();
        draw_frame(&engine);
    }
}