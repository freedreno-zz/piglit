// [config]
// expect_result: pass
// glsl_version: 1.20
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* PASS */
#ifndef GL_ES
#version 120
#endif

void main()
{
  struct s { int i; };

  s temp[] = s[](s(1), s(2));
}
