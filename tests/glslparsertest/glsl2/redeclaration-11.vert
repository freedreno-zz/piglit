// [config]
// expect_result: fail
// glsl_version: 1.20
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* FAIL - in 1.20, variables hide functions, so the call is illegal. */
#version 120

void foo(vec4 vs) {
   gl_Position = vs;
}

void main() {
   float foo = 1.0;
   foo(vec4(0.0));
}
