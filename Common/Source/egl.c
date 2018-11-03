#include "egl.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3ext.h>
/*
 * Print a list of extensions, with word-wrapping.
 */
static void print_extension_list(const char *ext) {
    const char indentString[] = "    ";
    const int indent = 4;
    const int max = 79;
    int width, i, j;

    if (!ext || !ext[0]) return;

    width = indent;
    printf("%s", indentString);
    i = j = 0;
    while (1) {
        if (ext[j] == ' ' || ext[j] == 0) {
            /* found end of an extension name */
            const int len = j - i;
            if (width + len > max) {
                /* start a new line */
                printf("\n");
                width = indent;
                printf("%s", indentString);
            }
            /* print the extension name between ext[i] and ext[j] */
            while (i < j) {
                printf("%c", ext[i]);
                i++;
            }
            /* either we're all done, or we'll continue with next extension */
            width += len + 1;
            if (ext[j] == 0) {
                break;
            } else {
                i++;
                j++;
                if (ext[j] == 0) break;
                printf(", ");
                width += 2;
            }
        }
        j++;
    }
    printf("\n");
}

static void info(EGLDisplay egl_dpy) {
    const char *s;

    s = eglQueryString(egl_dpy, EGL_VERSION);
    printf("EGL_VERSION: %s\n", s);

    s = eglQueryString(egl_dpy, EGL_VENDOR);
    printf("EGL_VENDOR: %s\n", s);

    s = eglQueryString(egl_dpy, EGL_EXTENSIONS);
    printf("EGL_EXTENSIONS: %s\n", s);
    print_extension_list((char *)s);

    s = eglQueryString(egl_dpy, EGL_CLIENT_APIS);
    printf("EGL_CLIENT_APIS: %s\n", s);

    printf("GL_VERSION: %s\n", (char *)glGetString(GL_VERSION));
    printf("GL_RENDERER: %s\n", (char *)glGetString(GL_RENDERER));
    printf("GL_EXTENSIONS:\n");
    print_extension_list((char *)glGetString(GL_EXTENSIONS));
}

static bool create_context(EGLDisplay display, int es_version,
                           EGLContext *out_context, EGLConfig *out_config) {
    EGLenum api;
    EGLint rend;
    const char *name;

    assert(es_version > 0);

    switch (es_version) {
        case 0:
            api = EGL_OPENGL_API;
            rend = EGL_OPENGL_BIT;
            name = "Desktop OpenGL";
            break;
        case 2:
            api = EGL_OPENGL_ES_API;
            rend = EGL_OPENGL_ES2_BIT;
            name = "GLES 2.x";
            break;
        case 3:
            api = EGL_OPENGL_ES_API;
            rend = EGL_OPENGL_ES3_BIT;
            name = "GLES 3.x";
            break;
        default:
            abort();
    }

    fprintf(stdout, "Trying to create %s context.\n", name);

    if (!eglBindAPI(api)) {
        fprintf(stdout, "Could not bind API!\n");
        return false;
    }

    EGLint attributes[] = {EGL_SURFACE_TYPE,
                           EGL_WINDOW_BIT,
                           EGL_RED_SIZE,
                           8,
                           EGL_GREEN_SIZE,
                           8,
                           EGL_BLUE_SIZE,
                           8,
                           EGL_ALPHA_SIZE,
                           8,
                           EGL_RENDERABLE_TYPE,
                           rend,
                           EGL_NONE};

    EGLint num_configs;
    if (!eglChooseConfig(display, attributes, NULL, 0, &num_configs)) {
        num_configs = 0;
    }

    EGLConfig *configs = calloc(1, sizeof(EGLConfig) * num_configs);
    if (!eglChooseConfig(display, attributes, configs, num_configs,
                         &num_configs)) {
        num_configs = 0;
    }

    if (!num_configs) {
        free(configs);
        fprintf(stderr, "Could not choose EGLConfig!\n");
        return false;
    }

    int chosen = 0;
    fprintf(stderr, "%d EGLConfig filterd, choose the first one!\n",
            num_configs);
    EGLConfig config = configs[chosen];

#if 0
    EGLint attr_value;
    struct attr2map_s {
        int attr;
        char name[64];
    } attr_desc[] = {
        {EGL_BUFFER_SIZE, "egl_buffer_size"},
        {EGL_RED_SIZE, "egl_red_size"},
        {EGL_GREEN_SIZE, "egl_green_size"},
        {EGL_BLUE_SIZE, "egl_blue_size"},
        {EGL_LUMINANCE_SIZE, "egl_luminance_size"},
        {EGL_ALPHA_SIZE, "egl_alpha_size"},
        {EGL_ALPHA_MASK_SIZE, "egl_alpha_mask_size"},
        {EGL_BIND_TO_TEXTURE_RGB, "egl_bind_to_texture_rgb"},
        {EGL_BIND_TO_TEXTURE_RGBA, "egl_bind_to_texture_rgba"},
        {EGL_COLOR_BUFFER_TYPE, "egl_color_buffer_type"},
        {EGL_CONFIG_CAVEAT, "egl_config_caveat"},
        {EGL_CONFIG_ID, "egl_config_id"},
        {EGL_CONFORMANT, "egl_conformant"},
        {EGL_DEPTH_SIZE, "egl_depth_size"},
        {EGL_LEVEL, "egl_level"},
        {EGL_MATCH_NATIVE_PIXMAP, "egl_match_native_pixmap"},
        {EGL_MAX_SWAP_INTERVAL, "egl_max_swap_interval"},
        {EGL_MIN_SWAP_INTERVAL, "egl_min_swap_interval"},
        {EGL_NATIVE_RENDERABLE, "egl_native_renderable"},
        {EGL_NATIVE_VISUAL_TYPE, "egl_native_visual_type"},
        {EGL_RENDERABLE_TYPE, "egl_renderable_type"},
        {EGL_SAMPLE_BUFFERS, "egl_sample_buffers"},
        {EGL_SAMPLES, "egl_samples"},
        {EGL_STENCIL_SIZE, "egl_stencil_size"},
        {EGL_SURFACE_TYPE, "egl_surface_type"},
        {EGL_TRANSPARENT_TYPE, "egl_transparent_type"},
        {EGL_TRANSPARENT_RED_VALUE, "egl_transparent_red_value"},
        {EGL_TRANSPARENT_GREEN_VALUE, "egl_transparent_green_value"},
        {EGL_TRANSPARENT_BLUE_VALUE, "egl_transparent_blue_value"},
        {EGL_NONE, ""},
    };
    struct attr2map_s *desc = attr_desc;
    while (desc->attr != EGL_NONE) {
        if (eglGetConfigAttrib(display, config, desc->attr, &attr_value) ==
            EGL_TRUE) {
            fprintf(stderr, "attr %s: %d\n", desc->name, attr_value);
        } else {
            fprintf(stderr, "attr %s error\n", desc->name);
        }
        desc++;
    }
#endif

    free(configs);

    EGLContext *egl_ctx = NULL;

    if (es_version) {
        EGLint attrs[] = {EGL_CONTEXT_CLIENT_VERSION, es_version, EGL_NONE};

        egl_ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    }

    if (!egl_ctx) {
        fprintf(stderr, "Could not create EGL context!\n");
        return false;
    }

    *out_context = egl_ctx;
    *out_config = config;
    return true;
}

bool init_egl(struct global_ctx_s *ctx) {
    struct global_ctx_s *p = ctx;
    fprintf(stdout, "Initializing EGL\n");
    p->egl.display = eglGetDisplay(p->gbm.device);
    if (p->egl.display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display.\n");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(p->egl.display, &major, &minor)) {
        fprintf(stderr, "Failed to initialize EGL.\n");
        return false;
    } else {
        fprintf(stderr, "EGL version %d.%d initialized.\n", major, minor);
    }

    EGLConfig config;
    int es_version = 3;
    if (!create_context(p->egl.display, es_version, &p->egl.context, &config))
        return false;
    fprintf(stdout, "Initializing EGL surface\n");

    p->egl.surface =
        eglCreateWindowSurface(p->egl.display, config, p->gbm.surface, NULL);
    if (p->egl.surface == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface.\n");
        return false;
    }

    if (!eglMakeCurrent(p->egl.display, p->egl.surface, p->egl.surface,
                        p->egl.context)) {
        fprintf(stderr, "Failed to call eglMakeCurrent.\n");
        return false;
    }

    info(p->egl.display);

    eglSwapInterval(p->egl.display, 0);

    return true;
}

