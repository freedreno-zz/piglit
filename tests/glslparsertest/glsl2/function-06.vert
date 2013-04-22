// [config]
// expect_result: pass
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* PASS */

vec4 foo(in float x, in float y, float z, float w)
{
  vec4 v;
  v.x = x;
  v.y = y;
  v.z = z;
  v.w = w;
  return v;
}

vec4 foo(in float x)
{
   vec4 v;
   v.x = x;
   v.y = x;
   v.z = x;
   v.w = x;
   return v;
}

void main()
{
  gl_Position = foo(1.0, 1.0, 1.0, 0.0);
  gl_Position = foo(2.0);
}
