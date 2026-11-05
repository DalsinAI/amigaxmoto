/*
 * gluScaleImage and gluBuild2DMipmaps, as X-Moto uses them: 8-bit RGB or
 * RGBA pixels only (src/common/VTexture.cpp asks for nothing else).
 * Scaling takes the nearest pixel; each mipmap level is the box average of
 * the one above, uploaded with glTexImage2D, as GLU does.
 * The AmiCradle Team. GPL 2 or later, like X-Moto.
 */
#include <stdlib.h>
#include <string.h>
#include <GL/glu.h>

#define GLU_INVALID_ENUM 100900
#define GLU_INVALID_VALUE 100901
#define GLU_OUT_OF_MEMORY 100902

static int components(GLenum format)
{
	switch (format) {
	case GL_RGB: return 3;
	case GL_RGBA: return 4;
	case GL_LUMINANCE: case GL_ALPHA: return 1;
	case GL_LUMINANCE_ALPHA: return 2;
	default: return 0;
	}
}

GLint gluScaleImage(GLenum format, GLsizei wIn, GLsizei hIn, GLenum typeIn,
                    const void *dataIn, GLsizei wOut, GLsizei hOut,
                    GLenum typeOut, GLvoid *dataOut)
{
	const unsigned char *src = (const unsigned char *)dataIn;
	unsigned char *dst = (unsigned char *)dataOut;
	int n = components(format);
	GLsizei x, y;

	if (n == 0 || typeIn != GL_UNSIGNED_BYTE || typeOut != GL_UNSIGNED_BYTE)
		return GLU_INVALID_ENUM;
	if (wIn <= 0 || hIn <= 0 || wOut <= 0 || hOut <= 0)
		return GLU_INVALID_VALUE;

	for (y = 0; y < hOut; y++) {
		const unsigned char *row = src + (size_t)((long)y * hIn / hOut) * wIn * n;
		for (x = 0; x < wOut; x++) {
			const unsigned char *p = row + (size_t)((long)x * wIn / wOut) * n;
			memcpy(dst, p, n);
			dst += n;
		}
	}
	return 0;
}

GLint gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width,
                        GLsizei height, GLenum format, GLenum type,
                        const void *data)
{
	int n = components(format);
	unsigned char *cur, *next;
	GLsizei w = width, h = height;
	GLint level = 0;

	if (n == 0 || type != GL_UNSIGNED_BYTE)
		return GLU_INVALID_ENUM;
	if (w <= 0 || h <= 0)
		return GLU_INVALID_VALUE;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(target, 0, internalFormat, w, h, 0, format, type, data);

	cur = (unsigned char *)data;
	while (w > 1 || h > 1) {
		GLsizei nw = w > 1 ? w / 2 : 1;
		GLsizei nh = h > 1 ? h / 2 : 1;
		GLsizei x, y;
		int c;

		next = (unsigned char *)malloc((size_t)nw * nh * n);
		if (!next) {
			if (cur != data)
				free(cur);
			return GLU_OUT_OF_MEMORY;
		}
		for (y = 0; y < nh; y++) {
			GLsizei y0 = y * h / nh, y1 = h > 1 ? y0 + 1 : y0;
			for (x = 0; x < nw; x++) {
				GLsizei x0 = x * w / nw, x1 = w > 1 ? x0 + 1 : x0;
				const unsigned char *a = cur + ((size_t)y0 * w + x0) * n;
				const unsigned char *b = cur + ((size_t)y0 * w + x1) * n;
				const unsigned char *d = cur + ((size_t)y1 * w + x0) * n;
				const unsigned char *e = cur + ((size_t)y1 * w + x1) * n;
				unsigned char *o = next + ((size_t)y * nw + x) * n;
				for (c = 0; c < n; c++)
					o[c] = (unsigned char)((a[c] + b[c] + d[c] + e[c] + 2) >> 2);
			}
		}
		if (cur != data)
			free(cur);
		cur = next;
		w = nw;
		h = nh;
		level++;
		glTexImage2D(target, level, internalFormat, w, h, 0, format, type, cur);
	}
	if (cur != data)
		free(cur);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	return 0;
}
