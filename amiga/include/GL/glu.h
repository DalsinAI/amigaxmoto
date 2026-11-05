/*
 * The OpenGPU kit has no GLU. X-Moto calls two of its functions, in
 * src/common/VTexture.cpp; amiga/amiga_glu.c has them.
 * The AmiCradle Team. GPL 2 or later, like X-Moto.
 */
#ifndef AMIGA_GLU_H
#define AMIGA_GLU_H

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

GLint gluScaleImage(GLenum format, GLsizei wIn, GLsizei hIn, GLenum typeIn,
                    const void *dataIn, GLsizei wOut, GLsizei hOut,
                    GLenum typeOut, GLvoid *dataOut);
GLint gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width,
                        GLsizei height, GLenum format, GLenum type,
                        const void *data);

#ifdef __cplusplus
}
#endif

#endif
