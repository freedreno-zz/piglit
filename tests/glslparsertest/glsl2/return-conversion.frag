// [config]
// expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* FAIL - no implicit conversions for return values */
float foo()
{
   int x = 1;
   return x;
}
void main()
{
   gl_FragColor = vec4(0.0, foo(), 0.0, 0.0);
}
