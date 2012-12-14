// [config]
// expect_result: pass
// glsl_version: 1.20
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]
#ifndef GL_ES
#version 120
#endif

uniform mat2 m2;
varying mat4 m4;
void main()
{
   m4 = mat4(m2);
}
