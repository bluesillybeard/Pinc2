// This example demonstrates use with OpenGL

#include "example.h"
#include <pinc_opengl.h>

// Our own mini GL.h with function pointers

// Sorry for this dumb calling convention garbage
#if defined(_WIN32) && !defined(__WIN32__) && !defined(__CYGWIN__)
#define __WIN32__
#endif

#if defined(__WIN32__) && !defined(__CYGWIN__)
#  if defined(__MINGW32__) && defined(GL_NO_STDCALL) || defined(UNDER_CE)  /* The generated DLLs by MingW with STDCALL are not compatible with the ones done by Microsoft's compilers */
#    define APIENTRY
#  else
#    define APIENTRY __stdcall
#  endif
#elif defined(__CYGWIN__) && defined(USE_OPENGL32) /* use native windows opengl32 */
#  define APIENTRY __stdcall
#elif (defined(__GNUC__) && __GNUC__ >= 4) || (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590))
#  define APIENTRY
#endif /* WIN32 && !CYGWIN */

/*
 * WINDOWS: Include windows.h here to define APIENTRY.
 * It is also useful when applications include this file by
 * including only glut.h, since glut.h depends on windows.h.
 * Applications needing to include windows.h with parms other
 * than "WIN32_LEAN_AND_MEAN" may include windows.h before
 * glut.h or gl.h.
 */
#if defined(_WIN32) && !defined(APIENTRY) && !defined(__CYGWIN__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

#define GL_COLOR_BUFFER_BIT			0x00004000
#define GL_TRIANGLES				0x0004

// TODO: make sure the ABI matches for all platforms - OpenGL can be a bit annoying on that front...
typedef void (APIENTRY* PFN_glClearColor)(float red, float green, float blue, float alpha);
typedef void (APIENTRY* PFN_glClear)(unsigned int flags);
typedef void (APIENTRY* PFN_glBegin)(unsigned int mode);
typedef void (APIENTRY* PFN_glEnd)(void);
typedef void (APIENTRY* PFN_glVertex2f)(float x, float y);
typedef void (APIENTRY* PFN_glColor4f)(float red, float green, float blue, float alpha);
typedef void (APIENTRY* PFN_glViewport)(int x, int y, int width, int height);

static PFN_glClearColor glClearColor;
static PFN_glClear glClear;
static PFN_glBegin glBegin;
static PFN_glEnd glEnd;
static PFN_glVertex2f glVertex2f;
static PFN_glColor4f glColor4f;
static PFN_glViewport glViewport;

int main(void) {
    pinc_preinit_set_error_callback(example_error_callback);
    pinc_incomplete_init();

    // Init pinc with the opengl API.
    if(pinc_complete_init(pinc_window_backend_any, pinc_graphics_api_opengl, 0, 1, 0) == pinc_return_code_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }

    if(pinc_query_opengl_version_supported(pinc_window_backend_any, 2, 1, false) == pinc_opengl_support_status_none) {
        fprintf(stderr, "Support for OpenGL 1.2 is required.\n");
        return 100;
    }

    pinc_window window = pinc_window_create_incomplete();
    // Enter 0 for length so pinc does the work of finding the length for us
    pinc_window_set_title(window, "Pinc OpenGL example", 0);
    if(pinc_window_complete(window) == pinc_return_code_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }

    // Make OpenGL context - Just like with other Pinc objects, an OpenGL context is created, settings are set, then it is completed.
    // However, this Pinc opengl context IS NOT a real Pinc object. It cannot be put into other Pinc functions.
    // If you are familiar with OpenGL, you may be interested to here that the OpenGL context has no immediate binding to a window.
    // On many backends, this is done through a fake / dummy window. But, some backends (ex: Xlib) can create an OpenGl context plainly.
    // Note that most OpenGL calls will not work if the context is not bound to a window.
    pinc_opengl_context gl_context = pinc_opengl_create_context_incomplete();
    pinc_opengl_set_context_version(gl_context, 1, 2, false, false);
    if(pinc_opengl_context_complete(gl_context) == pinc_return_code_error) {
        return 100;
    }

    if(pinc_opengl_make_current(window, gl_context) == pinc_return_code_error) {
        return 100;
    }

    // Grab some OpenGL functions

    glClearColor = (PFN_glClearColor) pinc_opengl_get_proc("glClearColor");
    glClear = (PFN_glClear) pinc_opengl_get_proc("glClear");
    glBegin = (PFN_glBegin) pinc_opengl_get_proc("glBegin");
    glEnd = (PFN_glEnd) pinc_opengl_get_proc("glEnd");
    glVertex2f = (PFN_glVertex2f) pinc_opengl_get_proc("glVertex2f");
    glColor4f = (PFN_glColor4f) pinc_opengl_get_proc("glColor4f");
    glViewport = (PFN_glViewport) pinc_opengl_get_proc("glViewport");

    bool running = true;
    while(running) {
        pinc_step();
        if(pinc_event_window_closed(window)) {
            running = false;
        }
        if(pinc_event_window_resized(window)) {
            glViewport(0, 0, (int)pinc_window_get_width(window), (int)pinc_window_get_height(window));
        }
        // Draw a triangle with OpenGL.
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glBegin(GL_TRIANGLES);
            glColor4f(1.0, 0.0, 0.0, 1.0);
            glVertex2f(-0.5, -0.5);
            glColor4f(0.0, 1.0, 0.0, 1.0);
            glVertex2f(-0.5, 0.5);
            glColor4f(0.0, 0.0, 1.0, 1.0);
            glVertex2f(0.5, 0.5);
        glEnd();
        pinc_window_present_framebuffer(window);
    }
    pinc_opengl_context_deinit(gl_context);
    pinc_window_deinit(window);
    pinc_deinit();
}
