/* [config]
 * expect_result: pass
 * glsl_version: 1.20
 * glsles_version: 1.00
 * [end config]
 *
 * From page 20 (page 26 of the PDF) of the GLSL 1.20 spec:
 *
 *     "Arrays know the number of elements they contain. This can be obtained
 *     by using the length method:
 *
 *         a.length(); // returns 5 for the above declarations
 *
 *     The length method cannot be called on an array that has not been
 *     explicitly sized."
 */


uniform vec4 a[2];

void main()
{
  gl_Position = vec4(a.length());
}
