/* [config]
 * expect_result: fail
 * glsl_version: 1.20
 * glsles_version: 1.00
 * check_link: true
 * [end config]
 *
 * This test verifies that the fragment shader special variable
 * gl_ClipDistance (defined in GLSL 1.30) is not available when the
 * GLSL version is 1.20.
 */

varying float gl_ClipDistance[2];

void main()
{
}
