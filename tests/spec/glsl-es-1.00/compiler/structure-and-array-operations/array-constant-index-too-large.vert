/* [config]
 * expect_result: fail
 * glsl_version: 1.20
 * glsles_version: 1.00
 * [end config]
 *
 * From page 19 (page 25 of the PDF) of the GLSL 1.20 spec:
 *
 *     "It is illegal to declare an array with a size, and then later (in the
 *     same shader) index the same array with an integral constant expression
 *     greater than or equal to the declared size."
 */


uniform vec4 [6] an_array;

void main()
{
  gl_Position = an_array[6];
}
