/* [config]
 * expect_result: fail
 * glsl_version: 1.20
 * glsles_version: 1.00
 * [end config]
 */
void main () {
  gl_Position = vec4(0);
  outerProduct(mat3x3(0), mat3x3(0));
}
