/* A texture filter the driver refuses falls back to GL_NEAREST, for that texture only.
 *
 * What was seen. A Godot 4 game on the translation layer draws a scene with some of its content missing -- ground, grass, one
 * sprite -- while the rest of the scene draws and nothing errors: no resource or rendering error in the engine's log, a steady
 * 30 fps, and, with every GL entry point wrapped, one failure in 200,000 glBindTexture calls. That one failure is this:
 *
 *     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR) -> GL_INVALID_ENUM
 *     on texture 708, created as internal=GL_RGBA32F format=GL_RGBA type=GL_FLOAT 64x64
 *
 * Why. GL_RGBA32F is a 32-bit float texture, and GLES 3.0 does not allow filtering one without the driver's
 * OES_texture_float_linear. ANGLE reports the refusal as GL_INVALID_ENUM (strictly INVALID_OPERATION, since GL_LINEAR is a valid
 * enum), so it reads as a bad parameter rather than an unsupported format. Godot then leaves that texture's filter unset, and what
 * the texture draws has an undefined filter. An A15 device shows the same family as "Image format RGBAFloat not supported by
 * hardware, converting to RGBAHalf".
 *
 * It is not one game's problem: any Compatibility-renderer project that keeps HDR or light data in float textures asks for this, and
 * the more of a scene such a texture covers, the more of it can go blank.
 *
 * What this does. Only for GL_TEXTURE_MAG_FILTER and GL_TEXTURE_MIN_FILTER, only after the driver refused, and never when the value
 * asked for is already GL_NEAREST: set the same parameter to GL_NEAREST, the filter every texture can take. A texture that accepts
 * GL_LINEAR is not touched. The wrapper lives in k_adapt, the table eglGetProcAddress consults -- the path a GLES engine actually
 * takes to its GL entry points (glad loads them this way); wrappers registered anywhere else never see the engine's calls.
 *
 * What it does not do. In the run that led here this fired once in 59,724 texture parameters, so it does not account for whole layers
 * disappearing; that is a separate matter. This is the float-texture parameter fix on its own.
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
