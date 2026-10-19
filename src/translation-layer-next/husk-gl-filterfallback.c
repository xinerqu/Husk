/* A texture filter the driver refuses falls back to GL_NEAREST, for that texture only.
 *
 * ANGLE answers glTexParameteri(..., GL_TEXTURE_MAG_FILTER, GL_LINEAR) on a GL_RGBA32F texture with GL_INVALID_ENUM (it should be
 * GL_INVALID_OPERATION: a 32-bit float texture is not filterable without OES_texture_float_linear). Godot leaves the parameter
 * unset when that happens, and what the texture draws is then undefined -- in the report that led here, a 64x64 float texture's
 * layer went blank. The same family shows up on an A15 device as "Image format RGBAFloat not supported by hardware, converting to
 * RGBAHalf".
 *
 * The fallback runs only after the driver has refused: a texture that takes GL_LINEAR keeps it.
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
