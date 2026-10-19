/* A texture filter the driver refuses falls back to GL_NEAREST, for that texture only.
 *
 * What this fixes. A GL_RGBA32F (32-bit float) texture asked for GL_TEXTURE_MAG_FILTER = GL_LINEAR is refused by the driver, and
 * the parameter is then never set. GLES 3.0 does not allow filtering a float texture without OES_texture_float_linear; ANGLE answers
 * with GL_INVALID_ENUM (strictly INVALID_OPERATION, since GL_LINEAR is a valid enum), so it reads like a bad parameter rather than an
 * unsupported format. An A15 device shows the same family as "Image format RGBAFloat not supported by hardware, converting to
 * RGBAHalf".
 *
 *     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR) -> GL_INVALID_ENUM
 *     on a texture created as internal=GL_RGBA32F format=GL_RGBA type=GL_FLOAT
 *
 * Any Compatibility-renderer project that keeps HDR or light data in float textures asks for this, so it is not one game's problem.
 *
 * What this does not fix, and must not be read as fixing. This was found while investigating a different symptom -- a scene with its
 * ground, grass and one sprite missing while everything else drew, with no error from the engine and a steady 30 fps. That
 * investigation wrapped every GL entry point and found the total number of GL failures in the run to be one; a single refused
 * filter setting on a 64x64 texture in 59,724 texture parameters cannot account for whole layers disappearing. Closing that gap is
 * separate work. This branch only makes the parameter defined instead of unset.
 *
 * What it does. For GL_TEXTURE_MAG_FILTER and GL_TEXTURE_MIN_FILTER only, only after the driver refused, and never when the value
 * asked for is already GL_NEAREST: set the same parameter to GL_NEAREST, the filter every texture can take. A texture that accepts
 * GL_LINEAR is not touched. The wrapper lives in k_adapt, the table eglGetProcAddress consults -- the path a GLES engine actually
 * takes to its GL entry points (glad loads them this way); wrappers registered anywhere else never see the engine's calls.
 */
#define _DARWIN_C_SOURCE
#include <stdbool.h>

#include "husk-tl-bionic.h"

typedef unsigned GLenum;
typedef int GLint;

#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_NEAREST 0x2600
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_OPERATION 0x0502

static void (*R_TexParameteri)(GLenum, GLenum, GLint);
static GLenum (*R_GetError)(void);

void husk_gl_filter_set_real(void *tex_param_i, void *get_error)
{
    R_TexParameteri = (void (*)(GLenum, GLenum, GLint))tex_param_i;
    R_GetError = (GLenum (*)(void))get_error;
}

/* Called after the driver refused with err: true when the parameter was set as GL_NEAREST instead. */
bool husk_gl_filter_fallback(GLenum target, GLenum pname, GLint value, unsigned err)
{
    if (!R_TexParameteri || !R_GetError) return false;
    if (pname != GL_TEXTURE_MAG_FILTER && pname != GL_TEXTURE_MIN_FILTER) return false;
    if (err != GL_INVALID_ENUM && err != GL_INVALID_OPERATION) return false;
    if (value == GL_NEAREST) return false;                  /* that is the fallback; there is nothing left to try */

    R_TexParameteri(target, pname, GL_NEAREST);
    return R_GetError() == 0;
}
