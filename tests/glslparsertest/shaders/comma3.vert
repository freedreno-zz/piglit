// [config]
// expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

void main() 
{
    int i, j, k;
    float f;
    i = j, k, f;
    i = j = k, f = 1.0;
    i = j, k = (3, f);    // float cannot be assigned to int
    gl_Position = vec4(1);
}
