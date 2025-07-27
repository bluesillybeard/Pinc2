// This example demonstrates use with OpenGL shared contexts

// NOLINTBEGIN: As examples, these are really only subject to what matters for demonstrating features, not for 'real' code

#include "example.h"
#include "pinc.h"
#include <pinc_opengl.h>

// Our own mini GL.h with function pointers

// Sorry for this dumb calling convention garbage
// More or less directly copied from whatever gl.h was in my compiler's include path at the time
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
#define GL_ARRAY_BUFFER                   0x8892
#define GL_STATIC_DRAW                    0x88E4
#define GL_FLOAT				0x1406
#define GL_VERTEX_ARRAY				0x8074
#define GL_COLOR_ARRAY				0x8076

// TODO: make sure the ABI matches for all platforms - OpenGL can be a bit annoying on that front...
typedef void (APIENTRY* PFN_glClearColor)(float red, float green, float blue, float alpha);
typedef void (APIENTRY* PFN_glClear)(unsigned int flags);
typedef void (APIENTRY* PFN_glViewport)(int x, int y, int width, int height);
typedef void (APIENTRY* PFN_glIndexPointer)(unsigned int type, int stride, void* pointer);
typedef void (APIENTRY* PFN_glVertexPointer)(int size, unsigned int type, int stride, void* pointer);
typedef void (APIENTRY* PFN_glColorPointer)(int size, unsigned int type, int stride, void* pointer);
typedef void (APIENTRY* PFN_glEnableClientState)(unsigned int array);
typedef void (APIENTRY* PFN_glDisableClientState)(unsigned int array);
typedef void (APIENTRY* PFN_glDrawArrays)(unsigned int mode, int first, int count);
typedef void (APIENTRY* PFN_glBindBuffer)(unsigned int target, unsigned int buffer);
typedef void (APIENTRY* PFN_glDeleteBuffers)(int n, unsigned int const* buffers);
typedef void (APIENTRY* PFN_glGenBuffers)(int n, unsigned int* buffers);
typedef void (APIENTRY* PFN_glBufferData)(unsigned int target, intptr_t size, void const* data, unsigned int usage);
typedef void (APIENTRY* PFN_glFinish)(void);

// Every context needs its own set of function pointers
typedef struct {
    PincOpenglContextHandle ctx;
    PFN_glViewport Viewport;
    PFN_glClearColor ClearColor;
    PFN_glClear Clear;
    PFN_glIndexPointer IndexPointer;
    PFN_glVertexPointer VertexPointer;
    PFN_glColorPointer ColorPointer;
    PFN_glEnableClientState EnableClientState;
    PFN_glDisableClientState DisableClientState;
    PFN_glDrawArrays DrawArrays;
    PFN_glBindBuffer BindBuffer;
    PFN_glGenBuffers GenBuffers;
    PFN_glBufferData BufferData;
    PFN_glFinish Finish;
} GlContext;

// A little helper function
bool makeContext(bool share_with_current, GlContext* out_ctx) {
    PincOpenglContextHandle gl_context = pincOpenglCreateContextIncomplete();
    // We are going to share a buffer between the contexts - that requires at least version 1.5
    // Sure, we could use the buffer objects extension, but that is making this way too complicated for a simple example
    pincOpenglSetContextVersion(gl_context, 1, 5, PincOpenglContextProfile_core);
    // This flag tells pinc that it should set up this context to share with the current context
    pincOpenglSetContextShareWithCurrent(gl_context, share_with_current);
    // This is when the context is shared - so, whatever context is current when CompleteContext is called is the one that will be shared with.
    if(pincOpenglCompleteContext(gl_context) == PincReturnCode_error) {
        return false;
    }

    // We need the context to be current, but it doesn't need to be bound to a window
    // pincOpenglCompleteContext does not guarantee that the context is current afterwards
    if(pincOpenglMakeCurrent(0, gl_context) == PincReturnCode_error) {
        return false;
    }

    out_ctx->ctx = gl_context;
    out_ctx->Viewport = (PFN_glViewport)pincOpenglGetProc("glViewport");
    out_ctx->ClearColor = (PFN_glClearColor)pincOpenglGetProc("glClearColor");
    out_ctx->Clear = (PFN_glClear)pincOpenglGetProc("glClear");
    out_ctx->IndexPointer = (PFN_glIndexPointer)pincOpenglGetProc("glIndexPointer");
    out_ctx->VertexPointer = (PFN_glVertexPointer)pincOpenglGetProc("glVertexPointer");
    out_ctx->ColorPointer = (PFN_glColorPointer)pincOpenglGetProc("glColorPointer");
    out_ctx->EnableClientState = (PFN_glEnableClientState)pincOpenglGetProc("glEnableClientState");
    out_ctx->DisableClientState = (PFN_glDisableClientState)pincOpenglGetProc("glDisableClientState");
    out_ctx->DrawArrays = (PFN_glDrawArrays)pincOpenglGetProc("glDrawArrays");
    out_ctx->BindBuffer = (PFN_glBindBuffer)pincOpenglGetProc("glBindBuffer");
    out_ctx->GenBuffers = (PFN_glGenBuffers)pincOpenglGetProc("glGenBuffers");
    out_ctx->BufferData = (PFN_glBufferData)pincOpenglGetProc("glBufferData");
    out_ctx->Finish = (PFN_glFinish)pincOpenglGetProc("glFinish");
    return true;
}

int main(void) {
    pincPreinitSetErrorCallback(exampleErrorCallback);
    pincInitIncomplete();

    // Init pinc with the opengl API.
    if(pincInitComplete(PincWindowBackend_any, PincGraphicsApi_opengl, 0) == PincReturnCode_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }

    if(pincQueryOpenglVersionSupported(PincWindowBackend_any, 2, 1, PincOpenglContextProfile_core) == PincOpenglSupportStatus_none) {
        fprintf(stderr, "Support for OpenGL 1.5 is required.\n");
        return 100;
    }

    PincWindowHandle window = pincWindowCreateIncomplete();
    // Enter 0 for length so pinc does the work of finding the length for us
    pincWindowSetTitle(window, "Pinc OpenGL Shared contexts example", 0);
    if(pincWindowComplete(window) == PincReturnCode_error) {
        // Something went wrong. The error callback should have been called.
        return 100;
    }

    GlContext gl1;
    GlContext gl2;
    if(!makeContext(false, &gl1)) {
        return 100;
    }
    // In case makeContext changes in the future such that the context is not current.
    if(pincOpenglMakeCurrent(window, gl1.ctx) == PincReturnCode_error) {
        return 100;
    }
    if(!makeContext(true, &gl2)) {
        return 100;
    }

    // Create a vertex buffer in ctx1 and upload the data there, but actually use it in ctx2
    // Just to show that it's possible, we'll upload the data with "no" window
    // Pinc may still bind the context with a window for technical reasons, so don't be fooled when draw functions seem to mysteriously work.
    if(pincOpenglMakeCurrent(0, gl1.ctx) == PincReturnCode_error) {
        return 100;
    }
    unsigned int buffer;
    gl1.GenBuffers(1, &buffer);
    gl1.BindBuffer(GL_ARRAY_BUFFER, buffer);
    // Data for a colorful triangle
    // x, y, r, g, b
    float data[] = { 
        0, 0.5, 1, 0, 0,
        -0.5, -0.5, 0, 1, 0,
        0.5, -0.5, 0, 0, 1,
    };
    gl1.BufferData(GL_ARRAY_BUFFER, 3*5*sizeof(float), data, GL_STATIC_DRAW);
    // Make sure all of the data is uploaded before using it in the other context
    // Although OpenGL guarantees order of command execution within a single context, the same cannot be said when using shared contexts
    gl1.Finish();
    pincOpenglMakeCurrent(window, gl2.ctx);

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
            }
        }
        // Set up draw state - this IS NOT shared between contexts!
        // Only server state is shared between contexts, not client state.
        gl2.Viewport(0, 0, (int)pincWindowGetWidth(window), (int)pincWindowGetHeight(window));
        gl2.BindBuffer(GL_ARRAY_BUFFER, buffer);
        gl2.EnableClientState(GL_VERTEX_ARRAY);
        gl2.EnableClientState(GL_COLOR_ARRAY);
        gl2.VertexPointer(2, GL_FLOAT, 5*sizeof(float), 0);
        gl2.ColorPointer(3, GL_FLOAT, 5*sizeof(float), (void*)(2*sizeof(float)));
        // draw
        gl2.DrawArrays(GL_TRIANGLES, 0, 3);
        pincWindowPresentFramebuffer(window);
    }
    pincOpenglDeinitContext(gl1.ctx);
    pincOpenglDeinitContext(gl2.ctx);
    pincWindowDeinit(window);
    pincDeinit();
}

// NOLINTEND
