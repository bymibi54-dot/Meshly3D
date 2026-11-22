// No-op OpenGL ES implementation so the Renderer (buffer building, touch logic, drawing code paths)
// can run on a normal PC under sanitizers. It draws nothing.
#include <GLES3/gl3.h>
#include <android/log.h>

static GLuint g_next = 1;
static const GLubyte kVersion[] = "OpenGL ES 3.2 V@0502.0 (GIT@stub)";
static const GLubyte kGpu[] = "Stub GPU";

int __android_log_print(int, const char*, const char*, ...) { return 0; }
GLuint glCreateShader(GLenum) { return g_next++; }
void glShaderSource(GLuint, GLsizei, const char* const*, const GLint*) {}
void glCompileShader(GLuint) {}
void glGetShaderiv(GLuint, GLenum, GLint* p) { *p = 1; }
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, char*) {}
void glDeleteShader(GLuint) {}
GLuint glCreateProgram() { return g_next++; }
void glAttachShader(GLuint, GLuint) {}
void glLinkProgram(GLuint) {}
void glGetProgramiv(GLuint, GLenum, GLint* p) { *p = 1; }
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, char*) {}
void glDeleteProgram(GLuint) {}
const GLubyte* glGetString(GLenum n) { return n == GL_VERSION ? kVersion : kGpu; }
GLint glGetUniformLocation(GLuint, const char*) { return 1; }
void glGenVertexArrays(GLsizei n, GLuint* a) { for (GLsizei i = 0; i < n; ++i) a[i] = g_next++; }
void glBindVertexArray(GLuint) {}
void glGenBuffers(GLsizei n, GLuint* a) { for (GLsizei i = 0; i < n; ++i) a[i] = g_next++; }
void glBindBuffer(GLenum, GLuint) {}
void glBufferData(GLenum, GLsizeiptr, const void*, GLenum) {}
void glEnableVertexAttribArray(GLuint) {}
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {}
void glEnable(GLenum) {}
void glDisable(GLenum) {}
void glViewport(GLint, GLint, GLsizei, GLsizei) {}
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat) {}
void glClear(GLbitfield) {}
void glUseProgram(GLuint) {}
void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*) {}
void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat) {}
void glUniform3f(GLint, GLfloat, GLfloat, GLfloat) {}
void glUniform1f(GLint, GLfloat) {}
void glDrawArrays(GLenum, GLint, GLsizei) {}
void glDeleteBuffers(GLsizei, const GLuint*) {}
void glDeleteVertexArrays(GLsizei, const GLuint*) {}
void glPolygonOffset(GLfloat, GLfloat) {}
void glBlendFunc(GLenum, GLenum) {}
void glDepthFunc(GLenum) {}
