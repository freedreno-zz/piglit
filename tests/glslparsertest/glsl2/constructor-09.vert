// [config]
// expect_result: pass
// glsles_expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* PASS */

uniform int a;
uniform float b;
uniform bool c;

void main()
{
  float x;
  int y;
  bool z;

  x = float(a);
  x = float(b);
  x = float(c);

  y = int(a);
  y = int(b);
  y = int(c);

  z = bool(a);
  z = bool(b);
  z = bool(c);

  gl_Position = gl_Vertex;
}
