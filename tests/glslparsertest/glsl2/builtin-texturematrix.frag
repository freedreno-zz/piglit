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
void main()
{
    mat4  result;
    result = gl_TextureMatrix[0];
    result = gl_TextureMatrixInverse[0];
    result = gl_TextureMatrixTranspose[0];
    result = gl_TextureMatrixInverseTranspose[0];
}
