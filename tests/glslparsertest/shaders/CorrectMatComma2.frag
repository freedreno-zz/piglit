// [config]
// expect_result: pass
// glsl_version: 1.10
// glsles_version: 1.00
//
// # NOTE: Config section was auto-generated from file
// # NOTE: 'glslparser.tests' at git revision
// # NOTE: 6cc17ae70b70d150aa1751f8e28db7b2a9bd50f0
// [end config]

void main()
{
    const mat4 tempMat = mat4(1,0,0,1,1,0,1,0,0,0,0,0,0,0,0,0);

    int i = 0;
    int j = 1;

    vec4 v6 = tempMat[i,j]; 
    vec4 v8 = tempMat[j,2]; 
    mat4 m = mat4(1,0,0,1,1,0,1,0,0,0,0,0,0,0,0,0);
    vec4 v10 = m[j]; // 1,0,0,1 
    vec4 v11 = m[j,2]; 
    vec4 v12 = m[2,j]; 
     

    gl_FragColor = v6 + v8 + v10 + v11 + v12; 
}
