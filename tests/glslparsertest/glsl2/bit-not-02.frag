// [config]
// expect_result: fail
// glsl_version: 1.20
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* FAIL - bitwise operations aren't supported in 1.20. */
#version 120
void main()
{
    int x = ~0 - 1;
}
