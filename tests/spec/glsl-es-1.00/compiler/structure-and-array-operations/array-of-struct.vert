/* [config]
 * expect_result: pass
 * glsl_version: 1.20
 * glsles_version: 1.00
 * [end config]
 *
 * From page 19 (page 25 of the PDF) of the GLSL 1.20 spec:
 *
 *     "All basic types and structures can be formed into arrays."
 */


struct s {
  float x;
  int y;
};

void main()
{
  s a[2];
  gl_Position = vec4(a.length());
}
