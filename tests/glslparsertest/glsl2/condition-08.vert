// [config]
// expect_result: pass
// glsl_version: 1.10
// glsles_version: 1.00
// [end config]

uniform bool b;
float v = b ? 1.0 : 0.0;
void main() { gl_Position = vec4(v); }
