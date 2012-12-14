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

/* PASS - variables and functions have separate namespaces in 1.10 */
const float f = 1.0;

float f(float x)
{
  return pow(x, 2.1718281828);
}

void main()
{
  gl_Position = vec4(0.0);
}
