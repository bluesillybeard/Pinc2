// options only for library build
// No header guard needed in this case

// This is so cmake options work correctly when put directly as defines
#define ON 1
#define OFF 0

#ifndef PINC_HAVE_WINDOW_SDL2
# define PINC_HAVE_WINDOW_SDL2 1
#endif

#ifndef PINC_HAVE_GRAPHICS_RAW_OPENGL
# define PINC_HAVE_GRAPHICS_RAW_OPENGL 1
#endif

#ifndef PINC_ENABLE_ERROR_EXTERNAL
# define PINC_ENABLE_ERROR_EXTERNAL 1
#endif

#ifndef PINC_ENABLE_ERROR_ASSERT
# define PINC_ENABLE_ERROR_ASSERT 1
#endif

#ifndef PINC_ENABLE_ERROR_USER
# define PINC_ENABLE_ERROR_USER 1
#endif

#ifndef PINC_ENABLE_ERROR_SANITIZE
# define PINC_ENABLE_ERROR_SANITIZE 0
#endif

#ifndef PINC_ENABLE_ERROR_VALIDATE
# define PINC_ENABLE_ERROR_VALIDATE 0
#endif
