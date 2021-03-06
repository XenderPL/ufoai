/* gcc gldummy.c -shared -fpic -ldl -o libGL.so */
#include <GL/gl.h>

void glBindTexture (GLenum target, GLuint id)
{
}

GLenum glGetError (void)
{
	return GL_NO_ERROR;
}

void glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
}

void glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
		GLenum format, GLenum type, const GLvoid *texels)
{
}

void glGenTextures (GLsizei n, GLuint *textures)
{
	static GLuint texnum;
	for (GLsizei i = 0; i != n; ++i) {
		textures[i] = ++texnum;
	}
}
