/*
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "piglit-util.h"

/**
 * @file discard-api.c
 *
 * Tests basic API support for GL_RASTERIZER_DISCARD.
 *
 * From the EXT_transform_feedback spec:
 *
 *     "Accepted by the <cap> parameter of Enable, Disable, and
 *      IsEnabled, and by the <pname> parameter of GetBooleanv,
 *      GetIntegerv, GetFloatv, and GetDoublev:
 *
 *        RASTERIZER_DISCARD_EXT                            0x8C89"
 */

int piglit_width = 32;
int piglit_height = 32;
int piglit_window_mode = GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA;

enum piglit_result
piglit_display(void)
{
	/* UNREACHED */
	return PIGLIT_FAIL;
}

void
piglit_init(int argc, char **argv)
{
	GLint enabled;

	piglit_require_extension("GL_EXT_transform_feedback");

	glEnable(GL_RASTERIZER_DISCARD);
	if (!glIsEnabled(GL_RASTERIZER_DISCARD))
		piglit_report_result(PIGLIT_FAIL);
	glGetIntegerv(GL_RASTERIZER_DISCARD, &enabled);
	if (!enabled)
		piglit_report_result(PIGLIT_FAIL);
	piglit_check_gl_error(GL_NO_ERROR, PIGLIT_FAIL);

	glDisable(GL_RASTERIZER_DISCARD);
	if (glIsEnabled(GL_RASTERIZER_DISCARD))
		piglit_report_result(PIGLIT_FAIL);
	glGetIntegerv(GL_RASTERIZER_DISCARD, &enabled);
	if (enabled)
		piglit_report_result(PIGLIT_FAIL);
	piglit_check_gl_error(GL_NO_ERROR, PIGLIT_FAIL);

	piglit_report_result(PIGLIT_PASS);
}