// This example demonstrates use with OpenGL

// NOLINTBEGIN: As examples, these are really only subject to what matters for demonstrating features, not for 'real' code

#include "example.h"
#include "pinc.h"
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
    pincPreinitSetErrorCallback(exampleErrorCallback);
    pincInitIncomplete();

    // Init pinc with the opengl API.
    pincInitComplete(PincWindowBackend_any, PincGraphicsApi_opengl, 0);

    if(pincQueryOpenglVersionSupported(PincWindowBackend_any, 2, 1, PincOpenglContextProfile_core) == PincOpenglSupportStatus_none) {
        fprintf(stderr, "Support for OpenGL 1.2 is required.\n");
        return 100;
    }

    PincWindowHandle window = pincWindowCreateIncomplete();
    // Enter 0 for length so pinc does the work of finding the length for us
    pincWindowSetTitle(window, "Pinc OpenGL example", 0);
    pincWindowComplete(window);

    // Make OpenGL context - Just like with other Pinc objects, an OpenGL context is created, settings are set, then it is completed.
    // If you are familiar with OpenGL, you may be interested to here that the OpenGL context has no immediate binding to a window.
    // On many backends, this is done through a fake / dummy window. But, some backends (ex: Xlib) can create an OpenGl context plainly.
    // Note that most OpenGL calls will not work if the context is not bound to a window.
    PincOpenglContextHandle gl_context = pincOpenglCreateContextIncomplete();
    // This is actually the default in Pinc, but may as well set it anyway.
    pincOpenglSetContextVersion(gl_context, 1, 2, PincOpenglContextProfile_core);
    pincOpenglCompleteContext(gl_context);

    pincOpenglMakeCurrent(window, gl_context);

    // Grab some OpenGL functions

    glClearColor = (PFN_glClearColor) pincOpenglGetProc("glClearColor");
    glClear = (PFN_glClear) pincOpenglGetProc("glClear");
    glBegin = (PFN_glBegin) pincOpenglGetProc("glBegin");
    glEnd = (PFN_glEnd) pincOpenglGetProc("glEnd");
    glVertex2f = (PFN_glVertex2f) pincOpenglGetProc("glVertex2f");
    glColor4f = (PFN_glColor4f) pincOpenglGetProc("glColor4f");
    glViewport = (PFN_glViewport) pincOpenglGetProc("glViewport");

    bool running = true;
    while(running) {
        pincStep();
        uint32_t num_events = pincEventGetNum();
        for(uint32_t i=0; i<num_events; ++i) {
            switch(pincEventGetType(i)) {
                case PincEventType_closeSignal: {
                    // Just in case there are other windows that we don't know about
                    if(window == pincEventCloseSignalWindow(i)) {
                        running = false;
                        printf("Closed window\n");
                    }
                    break;
                }
                case PincEventType_resize: {
                    // Just in case there are other windows that we don't know about
                    if(window == pincEventResizeWindow(i)) {
                        glViewport(0, 0, (int)pincEventResizeWidth(i), (int)pincEventResizeHeight(i));
                    }
                    break;
                }
            }
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
        pincWindowPresentFramebuffer(window);
    }
    pincOpenglDeinitContext(gl_context);
    pincWindowDeinit(window);
    pincDeinit();
}

// NOLINTEND
