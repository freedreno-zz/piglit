/* [config]
 * expect_result: fail
 * glsl_version: 1.20
 * glsles_version: 1.00
 * [end config]
 *
 * From page 19 (page 25 of the PDF) of the GLSL 1.20 spec:
 *
 *     "Only one-dimensional arrays may be declared."
 */


uniform vec4 an_array[1][1];

void main()
{
  gl_Position = an_array[0][0];
}
