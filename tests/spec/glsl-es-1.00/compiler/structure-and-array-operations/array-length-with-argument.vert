/* [config]
 * expect_result: fail
 * glsl_version: 1.20
 * glsles_version: 1.00
 * [end config]
 */


uniform vec4 a[2];

void main()
{
  gl_Position = vec4(a.length(5));
}
