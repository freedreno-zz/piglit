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
 * From page 11 (page 17 of the PDF) of the GLSL 1.10 spec:
 *
 *     "[#pragma debug] can only be used outside function definitions."
 */
void main()
{
#pragma debug(off)
  gl_Position = gl_Vertex;
}
