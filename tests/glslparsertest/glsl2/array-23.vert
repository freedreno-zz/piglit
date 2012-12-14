// [config]
// expect_result: pass
// glsles_expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
// [end config]

void main()
{
    /* Array sizes greater than 256 trigger an assertion in the Mesa
       7.8 compiler.
     */
    int a[257];

    gl_Position = gl_Vertex;
}
