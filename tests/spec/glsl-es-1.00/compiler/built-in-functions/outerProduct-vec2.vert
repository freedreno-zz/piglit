/* [config]
 * expect_result: fail
 * glsl_version: 1.00
 * [end config]
 * outerProduct is not available in OpenGL SL 1.00. It is
 *   available in OpenGL SL 3.00.
 * http://www.khronos.org/opengles/sdk/docs/manglsl/xhtml/outerProduct.xml
 */
void main () {
  gl_Position = vec4(0);
  outerProduct(vec2(0), vec2(0));
}
