#pragma once
#include <cstddef>
typedef unsigned int GLuint; typedef unsigned int GLenum; typedef int GLint; typedef int GLsizei;
typedef float GLfloat; typedef unsigned char GLubyte; typedef ptrdiff_t GLsizeiptr; typedef unsigned char GLboolean; typedef unsigned int GLbitfield; typedef char GLchar;
#define GL_VERTEX_SHADER 1
#define GL_FRAGMENT_SHADER 2
#define GL_COMPILE_STATUS 3
#define GL_LINK_STATUS 4
#define GL_VERSION 5
#define GL_RENDERER 6
#define GL_ARRAY_BUFFER 7
#define GL_STATIC_DRAW 8
#define GL_FLOAT 9
#define GL_FALSE 0
#define GL_DEPTH_TEST 10
#define GL_COLOR_BUFFER_BIT 1
#define GL_DEPTH_BUFFER_BIT 2
#define GL_TRIANGLES 11
GLuint glCreateShader(GLenum); void glShaderSource(GLuint,GLsizei,const char* const*,const GLint*); void glCompileShader(GLuint);
void glGetShaderiv(GLuint,GLenum,GLint*); void glGetShaderInfoLog(GLuint,GLsizei,GLsizei*,char*); void glDeleteShader(GLuint);
GLuint glCreateProgram(); void glAttachShader(GLuint,GLuint); void glLinkProgram(GLuint); void glGetProgramiv(GLuint,GLenum,GLint*);
void glGetProgramInfoLog(GLuint,GLsizei,GLsizei*,char*); void glDeleteProgram(GLuint);
const GLubyte* glGetString(GLenum); GLint glGetUniformLocation(GLuint,const char*);
void glGenVertexArrays(GLsizei,GLuint*); void glBindVertexArray(GLuint); void glGenBuffers(GLsizei,GLuint*); void glBindBuffer(GLenum,GLuint);
void glBufferData(GLenum,GLsizeiptr,const void*,GLenum); void glEnableVertexAttribArray(GLuint);
void glVertexAttribPointer(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*); void glEnable(GLenum); void glViewport(GLint,GLint,GLsizei,GLsizei);
void glClearColor(GLfloat,GLfloat,GLfloat,GLfloat); void glClear(GLbitfield); void glUseProgram(GLuint);
void glUniformMatrix4fv(GLint,GLsizei,GLboolean,const GLfloat*); void glDrawArrays(GLenum,GLint,GLsizei);
#define GL_LINES 12
#define GL_POLYGON_OFFSET_FILL 13
void glDeleteBuffers(GLsizei,const GLuint*); void glDeleteVertexArrays(GLsizei,const GLuint*); void glDisable(GLenum);
void glPolygonOffset(GLfloat,GLfloat); void glUniform4f(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
#define GL_BLEND 14
#define GL_SRC_ALPHA 15
#define GL_ONE_MINUS_SRC_ALPHA 16
#define GL_POINTS 17
#define GL_LINE_LOOP 18
#define GL_LEQUAL 19
#define GL_STREAM_DRAW 20
void glBlendFunc(GLenum,GLenum); void glDepthFunc(GLenum); void glUniform1f(GLint,GLfloat); void glUniform3f(GLint,GLfloat,GLfloat,GLfloat);
