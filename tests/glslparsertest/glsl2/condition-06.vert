// [config]
// expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

/* FAIL
 *
 * From page 33 (page 39 of the PDF) of the GLSL 1.10 spec:
 *
 *    "The second and third expressions must be the same type, but can be of
 *    any type other than an array."
 */

uniform bool selector;
uniform vec4 a[2];
uniform vec4 b[2];
uniform int idx;

void main()
{
  gl_Position = (selector ? a : b)[idx];
}
