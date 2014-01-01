/*
 * The glheaders_xxx headerfiles are 
 * compile-time options for which support / wrapping library to use.
 * Given the wide variety in supported platforms, along with the stated
 * goal of keeping necessary dependencies and dynamic configurations
 * to a minimum, we opted for this approach when it comes to 
 * GL, extensions, video context etc.
 */
#ifndef ARCAN_GLHEADER
#define ARCAN_GLHEADER

#define GLEW_STATIC
#define GL_GLEXT_PROTOTYPES 1

#include <glew.h>
#include <GLFW/glfw3.h>

#endif
