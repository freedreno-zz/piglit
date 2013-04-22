/* [config]
 * expect_result: fail
 * glsl_version: 1.20
 * glsles_version: 1.00
 * [end config]
 *
 * From page 19 (page 25 of the PDF) of the GLSL 1.20 spec:
 *
 *     "This type can be used anywhere any other type can be used, including
 *     as the return value from a function
 *
 *         float[5] foo() { }
 *
 *     as a constructor of an array
 *
 *         float[5](3.4, 4.2, 5.0, 5.2, 1.1)
 *
 *     as an unnamed parameter
 *
 *         void foo(float[5])"
 */


vec4[] a_function(vec4 [6] p);

uniform vec4 [6] an_array;

void main()
{
  gl_Position = a_function(an_array)[0];
}
