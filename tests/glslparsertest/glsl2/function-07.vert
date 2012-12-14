// [config]
// expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* FAIL - function does not return a value */

vec4 foo(in float x)
{
   vec4 v;
   v.x = x;
   v.y = x;
   v.z = x;
   v.w = x;
}

void main()
{
  gl_Position = foo(2.0);
}
