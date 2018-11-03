//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// esUtil_X11.c
//
//    This file contains the LinuxX11 implementation of the windowing functions. 

///
// Includes
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include "esUtil.h"

#include "common.h"
#include "drm/drm_atomic.h"
#include "drm/drm_common.h"
#include "ta/ta_talloc.h"

#include <sys/time.h>


//////////////////////////////////////////////////////////////////
//
//  Private Functions
//
//


//////////////////////////////////////////////////////////////////
//
//  Public Functions
//
//

///
//  WinCreate()
//
//      This function initialized the native X11 display and window for EGL
//
//
static bool init_gbm(struct global_ctx_s *ctx) {
    struct global_ctx_s *p = ctx;
    fprintf(stdout, "Creating GBM device\n");
    p->gbm.device = gbm_create_device(p->kms->fd);
    if (!p->gbm.device) {
        fprintf(stderr, "Failed to create GBM device.\n");
        return false;
    }

    fprintf(stderr, "GBM backend: %s\n",
            gbm_device_get_backend_name(p->gbm.device));

    fprintf(stdout, "Initializing GBM surface (%d x %d)\n",
            p->kms->mode.hdisplay, p->kms->mode.vdisplay);
    p->gbm.surface = gbm_surface_create(
        p->gbm.device, p->kms->mode.hdisplay, p->kms->mode.vdisplay,
        GBM_FORMAT_BGRA8888,  // drm_fourcc.h defs should be gbm-compatible
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!p->gbm.surface) {
        fprintf(stderr, "Failed to create GBM surface.\n");
        return false;
    }
    return true;
}

static bool probe_primary_plane_format(struct global_ctx_s *ctx) {
    struct global_ctx_s *p = ctx;

    if (!p->kms->atomic_context) {
        p->primary_plane_format = DRM_FORMAT_XRGB8888;
        MP_VERBOSE(ctx,
                   "Not using DRM Atomic: Use DRM_FORMAT_XRGB8888 for primary "
                   "plane.\n");
        return true;
    }

    drmModePlane *drmplane =
        drmModeGetPlane(p->kms->fd, p->kms->atomic_context->primary_plane->id);
    bool have_argb8888 = false;
    bool have_xrgb8888 = false;
    bool result = false;
    for (unsigned int i = 0; i < drmplane->count_formats; ++i) {
        if (drmplane->formats[i] == DRM_FORMAT_ARGB8888) {
            have_argb8888 = true;
        } else if (drmplane->formats[i] == DRM_FORMAT_XRGB8888) {
            have_xrgb8888 = true;
        }
    }

    if (have_argb8888) {
        p->primary_plane_format = DRM_FORMAT_ARGB8888;
        MP_VERBOSE(ctx, "DRM_FORMAT_ARGB8888 supported by primary plane.\n");
        result = true;
    } else if (have_xrgb8888) {
        p->primary_plane_format = DRM_FORMAT_XRGB8888;
        MP_VERBOSE(ctx,
                   "DRM_FORMAT_ARGB8888 not supported by primary plane: "
                   "Falling back to DRM_FORMAT_XRGB8888.\n");
        result = true;
    }

    drmModeFreePlane(drmplane);
    return result;
}

static bool init_kms(struct global_ctx_s *ctx) {
    fprintf(stdout, "Initializing KMS\n");
    ctx->kms = kms_create(ctx->log, NULL, 0, 0);
    if (!ctx->kms) {
        MP_ERR(ctx, "Failed to create KMS.\n");
        return false;
    }

    if (!probe_primary_plane_format(ctx)) {
        MP_ERR(ctx, "No suitable format found on DRM primary plane.\n");
        return false;
    }

    return true;
}

static void framebuffer_destroy_callback(struct gbm_bo *bo, void *data) {
    struct framebuffer *fb = data;
    if (fb) {
        drmModeRmFB(fb->fd, fb->id);
    }
}

static void update_framebuffer_from_bo(struct global_ctx_s *ctx,
                                       struct gbm_bo *bo) {
    struct global_ctx_s *p = ctx;
    struct framebuffer *fb = gbm_bo_get_user_data(bo);
    if (fb) {
        p->fb = fb;
        return;
    }

    fb = talloc_zero(ctx, struct framebuffer);
    fb->fd = p->kms->fd;
    fb->width = gbm_bo_get_width(bo);
    fb->height = gbm_bo_get_height(bo);
    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;

    int ret = drmModeAddFB2(
        fb->fd, fb->width, fb->height, p->primary_plane_format,
        (uint32_t[4]){handle, 0, 0, 0}, (uint32_t[4]){stride, 0, 0, 0},
        (uint32_t[4]){0, 0, 0, 0}, &fb->id, 0);

    if (ret) {
        MP_ERR(ctx, "Failed to create framebuffer: %s\n", mp_strerror(errno));
    }
    gbm_bo_set_user_data(bo, fb, framebuffer_destroy_callback);
    p->fb = fb;
}

static bool crtc_setup(struct global_ctx_s *ctx) {
    struct global_ctx_s *p = ctx;
    if (p->active) return true;
    p->old_crtc = drmModeGetCrtc(p->kms->fd, p->kms->crtc_id);
    int ret =
        drmModeSetCrtc(p->kms->fd, p->kms->crtc_id, p->fb->id, 0, 0,
                       &p->kms->connector->connector_id, 1, &p->kms->mode);
    p->active = true;
    return ret == 0;
}

static void crtc_release(struct global_ctx_s *ctx) {
    struct global_ctx_s *p = ctx;

    if (!p->active) return;
    p->active = false;

    if (p->old_crtc) {
        drmModeSetCrtc(p->kms->fd, p->old_crtc->crtc_id, p->old_crtc->buffer_id,
                       p->old_crtc->x, p->old_crtc->y,
                       &p->kms->connector->connector_id, 1, &p->old_crtc->mode);
        drmModeFreeCrtc(p->old_crtc);
        p->old_crtc = NULL;
    }
}

static void drm_egl_swap_buffers(struct global_ctx_s *ctx) {
    struct global_ctx_s *p = ctx;
    struct drm_atomic_context *atomic_ctx = p->kms->atomic_context;
    int ret;

    eglSwapBuffers(p->egl.display, p->egl.surface);
    //p->gbm.bo = gbm_surface_lock_front_buffer(p->gbm.surface);
    p->waiting_for_flip = true;
    //update_framebuffer_from_bo(ctx, p->gbm.bo);

    atomic_ctx = NULL;
    if (atomic_ctx) {
        drm_object_set_property(atomic_ctx->request, atomic_ctx->primary_plane,
                                "FB_ID", p->fb->id);
        drm_object_set_property(atomic_ctx->request, atomic_ctx->primary_plane,
                                "CRTC_ID", atomic_ctx->crtc->id);
        drm_object_set_property(atomic_ctx->request, atomic_ctx->primary_plane,
                                "ZPOS", 1);

        ret = drmModeAtomicCommit(
            p->kms->fd, atomic_ctx->request,
            DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, NULL);
        if (ret) MP_WARN(ctx, "Failed to commit atomic request (%d)\n", ret);
    } else {
        ret = drmModePageFlip(p->kms->fd, p->kms->crtc_id, p->fb->id,
                              DRM_MODE_PAGE_FLIP_EVENT, p);
        if (ret) {
            MP_WARN(ctx, "Failed to queue page flip: %s\n", mp_strerror(errno));
        }
    }

    // poll page flip finish event
    const int timeout_ms = 3000;
    struct pollfd fds[1] = {{.events = POLLIN, .fd = p->kms->fd}};
    poll(fds, 1, timeout_ms);
    if (fds[0].revents & POLLIN) {
        ret = drmHandleEvent(p->kms->fd, &p->ev);
        if (ret != 0) {
            MP_ERR(ctx, "drmHandleEvent failed: %i\n", ret);
            p->waiting_for_flip = false;
            return;
        }
    }
    p->waiting_for_flip = false;
    fprintf(stderr, "PageFlip finished\n");

    if (atomic_ctx) {
        drmModeAtomicFree(atomic_ctx->request);
        // p->drm_params.atomic_request = atomic_ctx->request = NULL;
    }

    // gbm_surface_release_buffer(p->gbm.surface, p->gbm.bo);
    // p->gbm.bo = p->gbm.next_bo;
    //
    eglSwapBuffers(p->egl.display, p->egl.surface);
}

EGLBoolean WinCreate(ESContext *esContext, const char *title)
{
    EGLConfig ecfg;
    EGLint num_config;

    struct global_ctx_s *gctx = talloc_zero(NULL, struct global_ctx_s);

    if (!init_kms(gctx)) {
        fprintf(stderr, "%s\n", "Failed to int kms");
    }

    init_gbm(gctx);


    esContext->platformData = gctx;

    // esContext->eglNativeWindow = (EGLNativeWindowType) win;
    esContext->eglNativeDisplay = gctx->gbm.device;
    esContext->width = gctx->kms->mode.hdisplay;
    esContext->height = gctx->kms->mode.vdisplay;
    return EGL_TRUE;
}

///
//  userInterrupt()
//
//      Reads from X11 event loop and interrupt program if there is a keypress, or
//      window close action.
//
GLboolean userInterrupt(ESContext *esContext)
{
    GLboolean userinterrupt = GL_FALSE;
    return userinterrupt;
}

int platform_preinit( ESContext *esContext ) {
    struct global_ctx_s *gctx = (struct global_ctx_s *)esContext->platformData;
    // required by gbm_surface_lock_front_buffer
    eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
    MP_VERBOSE(gctx, "Preparing framebuffer\n");
    gctx->gbm.bo = gbm_surface_lock_front_buffer(gctx->gbm.surface);
    if (!gctx->gbm.bo) {
        MP_ERR(gctx, "Failed to lock GBM surface.\n");
        return false;
    }
    update_framebuffer_from_bo(gctx, gctx->gbm.bo);
    if (!gctx->fb || !gctx->fb->id) {
        MP_ERR(gctx, "Failed to create framebuffer.\n");
        return false;
    }

    if (!crtc_setup(gctx)) {
        MP_ERR(gctx, "Failed to set CRTC for connector %u: %s\n",
               gctx->kms->connector->connector_id, mp_strerror(errno));
        return false;
    }
}



///
//  WinLoop()
//
//      Start main windows loop
//
void WinLoop ( ESContext *esContext )
{
    struct timeval t1, t2;
    struct timezone tz;
    float deltatime;

    struct global_ctx_s *gctx = (struct global_ctx_s*)esContext->platformData;

    gettimeofday ( &t1 , &tz );

    while(userInterrupt(esContext) == GL_FALSE)
    {
        gettimeofday(&t2, &tz);
        deltatime = (float)(t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6);
        t1 = t2;

        if (esContext->updateFunc != NULL)
            esContext->updateFunc(esContext, deltatime);
        if (esContext->drawFunc != NULL)
            esContext->drawFunc(esContext);

        eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);        

		struct gbm_bo *next_bo;
		next_bo = gbm_surface_lock_front_buffer(gctx->gbm.surface);

		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

        struct timeval s, e;
        gettimeofday(&s, NULL);
        int ret;
        ret = drmModePageFlip(gctx->kms->fd, gctx->kms->crtc_id, gctx->fb->id,
                              DRM_MODE_PAGE_FLIP_EVENT, gctx);
        if (ret) {
            MP_WARN(gctx, "Failed to queue page flip: %s\n", mp_strerror(errno));
        }

        const int timeout_ms = 3000;
        struct pollfd fds[1] = {{.events = POLLIN, .fd = gctx->kms->fd}};
        poll(fds, 1, timeout_ms);
        if (fds[0].revents & POLLIN) {
            ret = drmHandleEvent(gctx->kms->fd, &gctx->ev);
            if (ret != 0) {
                MP_ERR(gctx, "drmHandleEvent failed: %i\n", ret);
                gctx->waiting_for_flip = false;
                return;
            }
        }
        gettimeofday(&e, NULL);
        fprintf(stderr, "PageFlip time %f ms\n", (double)(e.tv_usec - s.tv_usec) / 1000);
        
		/* release last buffer to render on again: */
		gbm_surface_release_buffer(gctx->gbm.surface, gctx->gbm.bo);
		gctx->gbm.bo = next_bo;
    }
}

///
//  Global extern.  The application must declsare this function
//  that runs the application.
//
extern int esMain( ESContext *esContext );

///
//  main()
//
//      Main entrypoint for application
//
int main ( int argc, char *argv[] )
{
   ESContext esContext;
   
   memset ( &esContext, 0, sizeof( esContext ) );


   if ( esMain ( &esContext ) != GL_TRUE )
      return 1;   
 
   WinLoop ( &esContext );

   if ( esContext.shutdownFunc != NULL )
	   esContext.shutdownFunc ( &esContext );

   if ( esContext.userData != NULL )
	   free ( esContext.userData );

   return 0;
}
