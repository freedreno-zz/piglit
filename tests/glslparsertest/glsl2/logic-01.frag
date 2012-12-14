// [config]
// expect_result: fail
// glsl_version: 1.10
// glsles_version: 1.00
// [end config]

/* Test short circuit evaluation: though the second argument isn't necessary
 * to determine the outcome of the branch, it must still be valid.
 */
void main()
{
	if (false && fhqwghads())
		gl_FragColor = vec4(0.0);
	else
		gl_FragColor = vec4(1.0);
}
