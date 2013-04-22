/*
 * Copyright © 2010 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define _GNU_SOURCE

#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#include <stdlib.h>
#else
#include <libgen.h>
#endif

#include "piglit-util-gl-common.h"
#include "piglit-vbo.h"

#include "shader_runner_gles_workarounds.h"

static void
get_required_versions(const char *script_name,
		      struct piglit_gl_test_config *config);
GLenum
decode_drawing_mode(const char *mode_str);

PIGLIT_GL_TEST_CONFIG_BEGIN

	if (argc > 1)
		get_required_versions(argv[1], &config);
	else
		config.supports_gl_compat_version = 10;
#if defined(PIGLIT_USE_OPENGL_ES2)
	config.supports_gl_es_version = 20;
#endif

	config.window_width = 250;
	config.window_height = 250;
	config.window_visual = PIGLIT_GL_VISUAL_RGBA | PIGLIT_GL_VISUAL_DOUBLE;

PIGLIT_GL_TEST_CONFIG_END

struct component_version {
	enum version_tag {
		VERSION_GL,
		VERSION_GLSL,
	} _tag;

	bool es;
	unsigned num;
	char _string[100];
};

extern float piglit_tolerance[4];

static struct component_version gl_version;
static struct component_version glsl_version;
static struct component_version glsl_req_version;
static int gl_max_fragment_uniform_components;
static int gl_max_vertex_uniform_components;

const char *path = NULL;
const char *test_start = NULL;

GLuint vertex_shaders[256];
unsigned num_vertex_shaders = 0;
GLuint geometry_shaders[256];
unsigned num_geometry_shaders = 0;
GLuint fragment_shaders[256];
unsigned num_fragment_shaders = 0;
int num_uniform_blocks;
GLuint *uniform_block_bos;
GLenum geometry_layout_input_type = GL_TRIANGLES;
GLenum geometry_layout_output_type = GL_TRIANGLE_STRIP;
GLint geometry_layout_vertices_out = 0;

char *shader_string;
GLint shader_string_size;
const char *vertex_data_start = NULL;
const char *vertex_data_end = NULL;
GLuint prog;
size_t num_vbo_rows = 0;
bool link_ok = false;
bool prog_in_use = false;
GLchar *prog_err_info = NULL;

enum states {
	none = 0,
	requirements,
	vertex_shader,
	vertex_shader_file,
	vertex_program,
	geometry_shader,
	geometry_shader_file,
	geometry_layout,
	fragment_shader,
	fragment_shader_file,
	fragment_program,
	vertex_data,
	test,
};


enum comparison {
	equal = 0,
	not_equal,
	less,
	greater_equal,
	greater,
	less_equal
};

bool
compare(float ref, float value, enum comparison cmp);

static void
version_init(struct component_version *v, enum version_tag tag, bool es, unsigned num)
{
	assert(tag == VERSION_GL || tag == VERSION_GLSL);

	v->_tag = tag;
	v->es = es;
	v->num = num;
	v->_string[0] = 0;
}

static void
version_copy(struct component_version *dest, struct component_version *src)
{
	memcpy(dest, src, sizeof(*dest));
}

static bool
version_compare(struct component_version *a, struct component_version *b, enum comparison cmp)
{
	assert(a->_tag == b->_tag);

	if (a->es != b->es)
		return false;

	return compare(a->num, b->num, cmp);
}

/**
 * Get the version string.
 */
static const char*
version_string(struct component_version *v)
{
	if (v->_string[0])
		return v->_string;

	switch (v->_tag) {
	case VERSION_GL:
		snprintf(v->_string, sizeof(v->_string) - 1, "GL%s %d.%d",
		         v->es ? " ES" : "",
		         v->num / 10, v->num % 10);
		break;
	case VERSION_GLSL:
		snprintf(v->_string, sizeof(v->_string) - 1, "GLSL%s %d.%d",
		         v->es ? " ES" : "",
		         v->num / 100, v->num % 100);
		break;
	default:
		assert(false);
		break;
	}

	return v->_string;
}

static GLboolean
string_match(const char *string, const char *line)
{
	return (strncmp(string, line, strlen(string)) == 0);
}


const char *
target_to_short_name(GLenum target)
{
	switch (target) {
	case GL_VERTEX_SHADER:
		return "VS";
	case GL_FRAGMENT_SHADER:
		return "FS";
	case GL_GEOMETRY_SHADER:
		return "GS";
	default:
		return "???";
	}
}


void
compile_glsl(GLenum target, bool release_text)
{
	GLuint shader = glCreateShader(target);
	GLint ok;

	switch (target) {
	case GL_VERTEX_SHADER:
		piglit_require_vertex_shader();
		break;
	case GL_FRAGMENT_SHADER:
		piglit_require_fragment_shader();
		break;
	case GL_GEOMETRY_SHADER:
		if (gl_version.num < 32)
			piglit_require_extension("GL_ARB_geometry_shader4");
		break;
	}

	if (!glsl_req_version.num) {
		printf("GLSL version requirement missing\n");
		piglit_report_result(PIGLIT_FAIL);
	}

	if (!strstr(shader_string, "#version ")) {
		char *shader_strings[2];
		char version_string[100];
		GLint shader_string_sizes[2];
		
		/* Add a #version directive based on the GLSL requirement. */
		sprintf(version_string, "#version %d", glsl_req_version.num);
		if (glsl_req_version.es && glsl_req_version.num != 100) {
			strcat(version_string, " es");
		}
		strcat(version_string, "\n");
		shader_strings[0] = version_string;
		shader_string_sizes[0] = strlen(version_string);
		shader_strings[1] = shader_string;
		shader_string_sizes[1] = shader_string_size;
		
		glShaderSource(shader, 2,
				    (const GLchar **) shader_strings,
				    shader_string_sizes);

	} else {
		glShaderSource(shader, 1,
				    (const GLchar **) &shader_string,
				    &shader_string_size);
	}

	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

	if (!ok) {
		GLchar *info;
		GLint size;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
		info = malloc(size);

		glGetShaderInfoLog(shader, size, NULL, info);

		fprintf(stderr, "Failed to compile %s: %s\n",
			target_to_short_name(target),
			info);

		free(info);
		piglit_report_result(PIGLIT_FAIL);
	}

	if (release_text) {
		free(shader_string);
	}

	switch (target) {
	case GL_VERTEX_SHADER:
		vertex_shaders[num_vertex_shaders] = shader;
		num_vertex_shaders++;
		break;
	case GL_GEOMETRY_SHADER:
		geometry_shaders[num_geometry_shaders] = shader;
		num_geometry_shaders++;
		break;
	case GL_FRAGMENT_SHADER:
		fragment_shaders[num_fragment_shaders] = shader;
		num_fragment_shaders++;
		break;
	}
}

void
compile_and_bind_program(GLenum target, const char *start, int len)
{
	GLuint prog;
	char *source;

	switch (target) {
	case GL_VERTEX_PROGRAM_ARB:
		piglit_require_extension("GL_ARB_vertex_program");
		break;
	case GL_FRAGMENT_PROGRAM_ARB:
		piglit_require_extension("GL_ARB_fragment_program");
		break;
	}

	source = malloc(len + 1);
	memcpy(source, start, len);
	source[len] = 0;
	prog = piglit_compile_program(target, source);

	glEnable(target);
	glBindProgramARB(target, prog);
	link_ok = true;
	prog_in_use = true;
}

/**
 * Copy a string until either whitespace or the end of the string
 */
const char *
strcpy_to_space(char *dst, const char *src)
{
	while (!isspace((int) *src) && (*src != '\0'))
		*(dst++) = *(src++);

	*dst = '\0';
	return src;
}


/**
 * Skip over whitespace upto the end of line
 */
const char *
eat_whitespace(const char *src)
{
	while (isspace((int) *src) && (*src != '\n'))
		src++;

	return src;
}


/**
 * Skip over non-whitespace upto the end of line
 */
const char *
eat_text(const char *src)
{
	while (!isspace((int) *src) && (*src != '\0'))
		src++;

	return src;
}


/**
 * Compare two values given a specified comparison operator
 */
bool
compare(float ref, float value, enum comparison cmp)
{
	switch (cmp) {
	case equal:         return value == ref;
	case not_equal:     return value != ref;
	case less:          return value <  ref;
	case greater_equal: return value >= ref;
	case greater:       return value >  ref;
	case less_equal:    return value <= ref;
	}

	assert(!"Should not get here.");
}


/**
 * Get the string representation of a comparison operator
 */
const char *
comparison_string(enum comparison cmp)
{
	switch (cmp) {
	case equal:         return "==";
	case not_equal:     return "!=";
	case less:          return "<";
	case greater_equal: return ">=";
	case greater:       return ">";
	case less_equal:    return "<=";
	}

	assert(!"Should not get here.");
}


void
load_shader_file(const char *line)
{
	GLsizei *const size = &shader_string_size;
	char buf[256];
	char *text;

	if (shader_string) {
		printf("Multiple shader files in same section: %s\n", line);
		piglit_report_result(PIGLIT_FAIL);
	}

	strcpy_to_space(buf, line);

	text = piglit_load_text_file(buf, (unsigned *) size);
	if ((text == NULL) && (path != NULL)) {
		const size_t len = strlen(path);

		memcpy(buf, path, len);
		buf[len] = '/';
		strcpy_to_space(&buf[len + 1], line);

		text = piglit_load_text_file(buf, (unsigned *) size);
	}

	if (text == NULL) {
		strcpy_to_space(buf, line);

		printf("could not load file \"%s\"\n", buf);
		piglit_report_result(PIGLIT_FAIL);
	}

	shader_string = text;
}


/**
 * Parse a binary comparison operator and return the matching token
 */
const char *
process_comparison(const char *src, enum comparison *cmp)
{
	char buf[32];

	switch (src[0]) {
	case '=':
		if (src[1] == '=') {
			*cmp = equal;
			return src + 2;
		}
		break;
	case '<':
		if (src[1] == '=') {
			*cmp = less_equal;
			return src + 2;
		} else {
			*cmp = less;
			return src + 1;
		}
	case '>':
		if (src[1] == '=') {
			*cmp = greater_equal;
			return src + 2;
		} else {
			*cmp = greater;
			return src + 1;
		}
	case '!':
		if (src[1] == '=') {
			*cmp = not_equal;
			return src + 2;
		}
		break;
	}

	strncpy(buf, src, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	printf("invalid comparison in test script:\n%s\n", buf);
	piglit_report_result(PIGLIT_FAIL);

	/* Won't get here. */
	return NULL;
}


/**
 * " ES" before the comparison operator indicates the version
 * pertains to GL ES.
 */
void
parse_version_comparison(const char *line, enum comparison *cmp,
			 struct component_version *v, enum version_tag tag)
{
	unsigned major;
	unsigned minor;
	unsigned full_num;
	bool es = false;

	if (string_match(" ES", line)) {
		es = true;
		line += 3;
	}
	line = eat_whitespace(line);
	line = process_comparison(line, cmp);

	line = eat_whitespace(line);
	sscanf(line, "%u.%u", &major, &minor);
	line = eat_text(line);

	line = eat_whitespace(line);
	if (*line != '\n') {
		printf("Unexpected characters following version comparison\n");
		piglit_report_result(PIGLIT_FAIL);
	}

	/* This hack is so that we can tell the difference between GL versions
	 * and GLSL versions.  All GL versions look like 3.2, and we want the
	 * integer to be 32.  All GLSL versions look like 1.40, and we want
	 * the integer to be 140.
	 */
	if (tag == VERSION_GLSL) {
		full_num = (major * 100) + minor;
	} else {
		full_num = (major * 10) + minor;
	}

	version_init(v, tag, es, full_num);
}

/**
 * Parse and check a line from the requirement section of the test
 */
void
process_requirement(const char *line)
{
	char buffer[4096];

	/* There are four types of requirements that a test can currently
	 * have:
	 *
	 *    * Require that some GL extension be supported
	 *    * Require some particular versions of GL
	 *    * Require some particular versions of GLSL
	 *    * Require some particular number of uniform components
	 *
	 * The tests for GL and GLSL versions can be equal, not equal,
	 * less, less-or-equal, greater, or greater-or-equal.  Extension tests
	 * can also require that a particular extension not be supported by
	 * prepending ! to the extension name.
	 */
	if (string_match("GL_MAX_FRAGMENT_UNIFORM_COMPONENTS", line)) {
		enum comparison cmp;
		int maxcomp;

		line = eat_whitespace(line + 34);

		line = process_comparison(line, &cmp);

		maxcomp = atoi(line);
		if (!compare(maxcomp, gl_max_fragment_uniform_components, cmp)) {
			printf("Test requires max fragment uniform components %s %i.  "
			       "The driver supports %i.\n",
			       comparison_string(cmp),
			       maxcomp,
			       gl_max_fragment_uniform_components);
			piglit_report_result(PIGLIT_SKIP);
		}
	} else if (string_match("GL_MAX_VERTEX_UNIFORM_COMPONENTS", line)) {
		enum comparison cmp;
		int maxcomp;

		line = eat_whitespace(line + 32);

		line = process_comparison(line, &cmp);

		maxcomp = atoi(line);
		if (!compare(maxcomp, gl_max_vertex_uniform_components, cmp)) {
			printf("Test requires max vertex uniform components %s %i.  "
			       "The driver supports %i.\n",
			       comparison_string(cmp),
			       maxcomp,
			       gl_max_vertex_uniform_components);
			piglit_report_result(PIGLIT_SKIP);
		}
	} else if (string_match("GL_", line)) {
		strcpy_to_space(buffer, line);
		piglit_require_extension(buffer);
	} else if (string_match("!GL_", line)) {
		strcpy_to_space(buffer, line + 1);
		piglit_require_not_extension(buffer);
	} else if (string_match("GLSL", line)) {
		enum comparison cmp;

		parse_version_comparison(line + 4, &cmp, &glsl_req_version,
		                         VERSION_GLSL);

		/* We only allow >= because we potentially use the
		 * version number to insert a #version directive. */
		if (cmp != greater_equal) {
			printf("Unsupported GLSL version comparison\n");
			piglit_report_result(PIGLIT_FAIL);
		}

		if (!version_compare(&glsl_req_version, &glsl_version, cmp)) {
			printf("Test requires %s %s.  "
			       "Actual version %s.\n",
			       comparison_string(cmp),
			       version_string(&glsl_req_version),
			       version_string(&glsl_version));
			piglit_report_result(PIGLIT_SKIP);
		}
	} else if (string_match("GL", line)) {
		enum comparison cmp;
		struct component_version gl_req_version;

		parse_version_comparison(line + 2, &cmp, &gl_req_version,
		                         VERSION_GL);

		if (!version_compare(&gl_req_version, &gl_version, cmp)) {
			printf("Test requires %s %s.  "
			       "Actual version is %s.\n",
			       comparison_string(cmp),
			       version_string(&gl_req_version),
			       version_string(&gl_version));
			piglit_report_result(PIGLIT_SKIP);
		}
	} else if (string_match("rlimit", line)) {
		unsigned long lim;
		char *ptr;

		line = eat_whitespace(line + 6);

		lim = strtoul(line, &ptr, 0);
		if (ptr == line) {
			printf("rlimit requires numeric argument\n");
			piglit_report_result(PIGLIT_FAIL);
		}

		piglit_set_rlimit(lim);
	}
}


/**
 * Process a line from the [geometry layout] section of a test
 */
void
process_geometry_layout(const char *line)
{
	char s[32];
	int x;

	line = eat_whitespace(line);

	if (line[0] == '\0' || line[0] == '\n') {
		return;
	} else if (sscanf(line, "input type %31s", s) == 1) {
		geometry_layout_input_type = decode_drawing_mode(s);
	} else if (sscanf(line, "output type %31s", s) == 1) {
		geometry_layout_output_type = decode_drawing_mode(s);
	} else if (sscanf(line, "vertices out %d", &x) == 1) {
		geometry_layout_vertices_out = x;
	} else {
		printf("Could not parse geometry layout line: %s\n", line);
		piglit_report_result(PIGLIT_FAIL);
	}
}


void
leave_state(enum states state, const char *line)
{
	switch (state) {
	case none:
		break;

	case requirements:
		break;

	case vertex_shader:
		shader_string_size = line - shader_string;
		compile_glsl(GL_VERTEX_SHADER, false);
		break;

	case vertex_shader_file:
		compile_glsl(GL_VERTEX_SHADER, true);
		break;

	case vertex_program:
		compile_and_bind_program(GL_VERTEX_PROGRAM_ARB,
					 shader_string,
					 line - shader_string);
		break;

	case geometry_shader:
		shader_string_size = line - shader_string;
		compile_glsl(GL_GEOMETRY_SHADER, false);
		break;

	case geometry_shader_file:
		compile_glsl(GL_GEOMETRY_SHADER, true);
		break;

	case geometry_layout:
		break;

	case fragment_shader:
		shader_string_size = line - shader_string;
		compile_glsl(GL_FRAGMENT_SHADER, false);
		break;

	case fragment_shader_file:
		compile_glsl(GL_FRAGMENT_SHADER, true);
		break;

	case fragment_program:
		compile_and_bind_program(GL_FRAGMENT_PROGRAM_ARB,
					 shader_string,
					 line - shader_string);
		break;

	case vertex_data:
		vertex_data_end = line;
		break;

	case test:
		break;

	default:
		assert(!"Not yet supported.");
	}
}


void
link_and_use_shaders(void)
{
	unsigned i;
	GLenum err;
	GLint ok;

	if ((num_vertex_shaders == 0)
	    && (num_fragment_shaders == 0)
	    && (num_geometry_shaders == 0))
		return;

	prog = glCreateProgram();

	for (i = 0; i < num_vertex_shaders; i++) {
		glAttachShader(prog, vertex_shaders[i]);
	}

	for (i = 0; i < num_geometry_shaders; i++) {
		glAttachShader(prog, geometry_shaders[i]);
	}

	for (i = 0; i < num_fragment_shaders; i++) {
		glAttachShader(prog, fragment_shaders[i]);
	}

#ifdef PIGLIT_USE_OPENGL
	if (geometry_layout_input_type != GL_TRIANGLES) {
		glProgramParameteriARB(prog, GL_GEOMETRY_INPUT_TYPE_ARB,
				       geometry_layout_input_type);
	}
	if (geometry_layout_output_type != GL_TRIANGLE_STRIP) {
		glProgramParameteriARB(prog, GL_GEOMETRY_OUTPUT_TYPE_ARB,
				       geometry_layout_output_type);
	}
	if (geometry_layout_vertices_out != 0) {
		glProgramParameteriARB(prog, GL_GEOMETRY_VERTICES_OUT_ARB,
				       geometry_layout_vertices_out);
	}
#endif

	glLinkProgram(prog);

	for (i = 0; i < num_vertex_shaders; i++) {
		glDeleteShader(vertex_shaders[i]);
	}

	for (i = 0; i < num_geometry_shaders; i++) {
		glDeleteShader(geometry_shaders[i]);
	}

	for (i = 0; i < num_fragment_shaders; i++) {
		glDeleteShader(fragment_shaders[i]);
	}

	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (ok) {
		link_ok = true;
	} else {
		GLint size;

		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &size);
		prog_err_info = malloc(size);

		glGetProgramInfoLog(prog, size, NULL, prog_err_info);

		return;
	}

	glUseProgram(prog);

	err = glGetError();
	if (!err) {
		prog_in_use = true;
	} else {
		GLint size;

		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &size);
		prog_err_info = malloc(size);

		glGetProgramInfoLog(prog, size, NULL, prog_err_info);
	}
}


void
process_test_script(const char *script_name)
{
	unsigned text_size;
	char *text = piglit_load_text_file(script_name, &text_size);
	enum states state = none;
	const char *line = text;

	if (line == NULL) {
		printf("could not read file \"%s\"\n", script_name);
		piglit_report_result(PIGLIT_FAIL);
	}

	while (line[0] != '\0') {
		if (line[0] == '[') {
			leave_state(state, line);

			if (string_match("[require]", line)) {
				state = requirements;
			} else if (string_match("[vertex shader]", line)) {
				state = vertex_shader;
				shader_string = NULL;
			} else if (string_match("[vertex program]", line)) {
				state = vertex_program;
				shader_string = NULL;
			} else if (string_match("[vertex shader file]", line)) {
				state = vertex_shader_file;
				shader_string = NULL;
			} else if (string_match("[geometry shader]", line)) {
				state = geometry_shader;
				shader_string = NULL;
			} else if (string_match("[geometry shader file]", line)) {
				state = vertex_shader_file;
				shader_string = NULL;
			} else if (string_match("[geometry layout]", line)) {
				state = geometry_layout;
				shader_string = NULL;
			} else if (string_match("[fragment shader]", line)) {
				state = fragment_shader;
				shader_string = NULL;
			} else if (string_match("[fragment program]", line)) {
				state = fragment_program;
				shader_string = NULL;
			} else if (string_match("[fragment shader file]", line)) {
				state = fragment_shader_file;
				shader_string = NULL;
			} else if (string_match("[vertex data]", line)) {
				state = vertex_data;
				vertex_data_start = NULL;
			} else if (string_match("[test]", line)) {
				test_start = strchrnul(line, '\n');
				if (test_start[0] != '\0')
					test_start++;
				return;
			}
		} else {
			switch (state) {
			case none:
				break;

			case requirements:
				process_requirement(line);
				break;

			case geometry_layout:
				process_geometry_layout(line);
				break;

			case vertex_shader:
			case vertex_program:
			case geometry_shader:
			case fragment_shader:
			case fragment_program:
				if (shader_string == NULL)
					shader_string = (char *) line;
				break;

			case vertex_shader_file:
			case geometry_shader_file:
			case fragment_shader_file:
				line = eat_whitespace(line);
				if ((line[0] != '\n') && (line[0] != '#'))
					load_shader_file(line);
				break;

			case vertex_data:
				if (vertex_data_start == NULL)
					vertex_data_start = line;
				break;

			case test:
				break;
			}
		}

		line = strchrnul(line, '\n');
		if (line[0] != '\0')
			line++;
	}

	leave_state(state, line);
}

struct requirement_parse_results {
	bool found_gl;
	bool found_glsl;
	struct component_version gl_version;
	struct component_version glsl_version;
};

static void
parse_required_versions(struct requirement_parse_results *results,
                        const char *script_name)
{
	unsigned text_size;
	char *text = piglit_load_text_file(script_name, &text_size);
	const char *line = text;
	bool in_requirement_section = false;

	results->found_gl = false;
	results->found_glsl = false;

	if (line == NULL) {
		printf("could not read file \"%s\"\n", script_name);
		piglit_report_result(PIGLIT_FAIL);
	}

	while (line[0] != '\0') {
		if (line[0] == '[') {
			if (in_requirement_section)
				break;
			else
				in_requirement_section = false;
		}

		if (!in_requirement_section) {
			if (string_match("[require]", line)) {
				in_requirement_section = true;
			}
		} else {
			if (string_match("GL_", line)
			    || string_match("!GL_", line)) {
				/* empty */
			} else if (string_match("GLSL", line)) {
				enum comparison cmp;
				struct component_version version;

				parse_version_comparison(line + 4, &cmp,
							 &version, VERSION_GLSL);
				if (cmp == greater_equal) {
					results->found_glsl = true;
					version_copy(&results->glsl_version, &version);
				}
			} else if (string_match("GL", line)) {
				enum comparison cmp;
				struct component_version version;

				parse_version_comparison(line + 2, &cmp,
							 &version, VERSION_GL);
				if (cmp == greater_equal
				    || cmp == greater
				    || cmp == equal) {
					results->found_gl = true;
					version_copy(&results->gl_version, &version);
				}
			}
		}

		line = strchrnul(line, '\n');
		if (line[0] != '\0')
			line++;
	}

	free(text);

	if (!in_requirement_section) {
		printf("[require] section missing\n");
		piglit_report_result(PIGLIT_FAIL);
	}

	if (results->found_glsl && results->glsl_version.es && !results->found_gl) {
		printf("%s", "The test specifies a requirement for GLSL ES, "
		       "but specifies no GL requirement\n.");
		piglit_report_result(PIGLIT_FAIL);
	}
}


static void
choose_required_gl_version(struct requirement_parse_results *parse_results,
                           struct component_version *gl_version)
{
	if (parse_results->found_gl) {
		version_copy(gl_version, &parse_results->gl_version);
	} else {
		assert(!parse_results->found_glsl || !parse_results->glsl_version.es);
		version_init(gl_version, VERSION_GL, false, 10);
	}

	if (gl_version->es)
		return;

	/* Possibly promote the GL version. */
	if (gl_version->num < required_gl_version_from_glsl_version(
			parse_results->glsl_version.num)) {
		gl_version->num = required_gl_version_from_glsl_version(
			parse_results->glsl_version.num);
	}
}

/**
 * Just determine the GLSL version required by the shader script.
 *
 * This function is a bit of a hack that is, unfortunately necessary.  A test
 * script can require a specific GLSL version or a specific GL version.  To
 * satisfy this requirement, the piglit framework code needs to know about the
 * requirement before creating the context.  However, the requirements section
 * can contain other requirements, such as minimum number of uniforms.
 *
 * The requirements section can't be fully processed until after the context
 * is created, but the context can't be created until after the requirements
 * section is processed.  Do a quick can over the requirements section to find
 * the GL and GLSL version requirements.  Use these to guide context creation.
 */
void
get_required_versions(const char *script_name,
		      struct piglit_gl_test_config *config)
{
	struct requirement_parse_results parse_results;
	struct component_version required_gl_version;

	parse_required_versions(&parse_results, script_name);
	choose_required_gl_version(&parse_results, &required_gl_version);

	if (required_gl_version.es) {
		config->supports_gl_es_version = required_gl_version.num;
	} else if (required_gl_version.num >= 31) {
		config->supports_gl_core_version = required_gl_version.num;
		config->supports_gl_compat_version = required_gl_version.num;
	} else {
		config->supports_gl_compat_version = 10;
	}
}

void
get_floats(const char *line, float *f, unsigned count)
{
	unsigned i;

	for (i = 0; i < count; i++)
		f[i] = strtod(line, (char **) &line);
}


void
get_ints(const char *line, int *ints, unsigned count)
{
	unsigned i;

	for (i = 0; i < count; i++)
		ints[i] = strtol(line, (char **) &line, 0);
}


void
get_uints(const char *line, unsigned *uints, unsigned count)
{
	unsigned i;

	for (i = 0; i < count; i++)
		uints[i] = strtoul(line, (char **) &line, 0);
}


/**
 * Check that the GL implementation supports unsigned uniforms
 * (e.g. through glUniform1ui).  If not, terminate the test with a
 * SKIP.
 */
void
check_unsigned_support()
{
	if (gl_version.num < 30)
		piglit_report_result(PIGLIT_SKIP);
}

/**
 * Handles uploads of UBO uniforms by mapping the buffer and storing
 * the data.  If the uniform is not in a uniform block, returns false.
 */
bool
set_ubo_uniform(const char *name, const char *type, const char *line)
{
	GLuint uniform_index;
	GLint block_index;
	GLint offset;
	char *data;
	float f[16];
	int ints[16];
	unsigned uints[16];
	int name_len = strlen(name);

	if (!num_uniform_blocks)
		return false;

	glGetUniformIndices(prog, 1, &name, &uniform_index);
	if (uniform_index == GL_INVALID_INDEX) {
		printf("cannot get index of uniform \"%s\"\n", name);
		piglit_report_result(PIGLIT_FAIL);
	}

	glGetActiveUniformsiv(prog, 1, &uniform_index,
			      GL_UNIFORM_BLOCK_INDEX, &block_index);

	if (block_index == -1)
		return false;

	glGetActiveUniformsiv(prog, 1, &uniform_index,
			      GL_UNIFORM_OFFSET, &offset);

	if (name[name_len - 1] == ']') {
		GLint stride;
		int i;

		for (i = name_len - 1; (i > 0) && isdigit(name[i-1]); --i)
			/* empty */;

		glGetActiveUniformsiv(prog, 1, &uniform_index,
				      GL_UNIFORM_ARRAY_STRIDE, &stride);
		offset += stride * strtol(&name[i], NULL, 0);
	}

	glBindBuffer(GL_UNIFORM_BUFFER,
		     uniform_block_bos[block_index]);
	data = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
	data += offset;

	if (string_match("float", type)) {
		get_floats(line, f, 1);
		memcpy(data, f, sizeof(float));
	} else if (string_match("int", type)) {
		get_ints(line, ints, 1);
		memcpy(data, ints, sizeof(int));
	} else if (string_match("uint", type)) {
		get_uints(line, uints, 1);
		memcpy(data, uints, sizeof(int));
	} else if (string_match("vec", type)) {
		int elements = type[3] - '0';
		get_floats(line, f, elements);
		memcpy(data, f, elements * sizeof(float));
	} else if (string_match("ivec", type)) {
		int elements = type[4] - '0';
		get_ints(line, ints, elements);
		memcpy(data, ints, elements * sizeof(int));
	} else if (string_match("uvec", type)) {
		int elements = type[4] - '0';
		get_uints(line, uints, elements);
		memcpy(data, uints, elements * sizeof(unsigned));
	} else if (string_match("mat", type)) {
		GLint matrix_stride, row_major;
		int cols = type[3] - '0';
		int rows = type[4] == 'x' ? type[5] - '0' : cols;
		int r, c;
		float *matrixdata = (float *)data;

		assert(cols >= 2 && cols <= 4);
		assert(rows >= 2 && rows <= 4);

		get_floats(line, f, rows * cols);

		glGetActiveUniformsiv(prog, 1, &uniform_index,
				      GL_UNIFORM_MATRIX_STRIDE, &matrix_stride);
		glGetActiveUniformsiv(prog, 1, &uniform_index,
				      GL_UNIFORM_IS_ROW_MAJOR, &row_major);

		matrix_stride /= sizeof(float);

		for (c = 0; c < cols; c++) {
			for (r = 0; r < rows; r++) {
				if (row_major) {
					matrixdata[matrix_stride * c + r] =
						f[r * rows + c];
				} else {
					matrixdata[matrix_stride * r + c] =
						f[r * rows + c];
				}
			}
		}
	} else {
		printf("unknown uniform type \"%s\" for \"%s\"", type, name);
		piglit_report_result(PIGLIT_FAIL);
	}

	glUnmapBuffer(GL_UNIFORM_BUFFER);

	return true;
}

void
set_uniform(const char *line)
{
	char name[512];
	float f[16];
	int ints[16];
	unsigned uints[16];
	GLuint prog;
	GLint loc;
	const char *type;

	glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *) &prog);

	type = eat_whitespace(line);
	line = eat_text(type);

	line = strcpy_to_space(name, eat_whitespace(line));

	if (set_ubo_uniform(name, type, line))
		return;

	loc = glGetUniformLocation(prog, name);
	if (loc < 0) {
		printf("cannot get location of uniform \"%s\"\n",
		       name);
		piglit_report_result(PIGLIT_FAIL);
	}

	if (string_match("float", type)) {
		get_floats(line, f, 1);
		glUniform1fv(loc, 1, f);
		return;
	} else if (string_match("int", type)) {
		int val = atoi(line);
		glUniform1i(loc, val);
		return;
	} else if (string_match("uint", type)) {
		unsigned val;
		check_unsigned_support();
		val = strtoul(line, NULL, 0);
		glUniform1ui(loc, val);
		return;
	} else if (string_match("vec", type)) {
		switch (type[3]) {
		case '2':
			get_floats(line, f, 2);
			glUniform2fv(loc, 1, f);
			return;
		case '3':
			get_floats(line, f, 3);
			glUniform3fv(loc, 1, f);
			return;
		case '4':
			get_floats(line, f, 4);
			glUniform4fv(loc, 1, f);
			return;
		}
	} else if (string_match("ivec", type)) {
		switch (type[4]) {
		case '2':
			get_ints(line, ints, 2);
			glUniform2iv(loc, 1, ints);
			return;
		case '3':
			get_ints(line, ints, 3);
			glUniform3iv(loc, 1, ints);
			return;
		case '4':
			get_ints(line, ints, 4);
			glUniform4iv(loc, 1, ints);
			return;
		}
	} else if (string_match("uvec", type)) {
		check_unsigned_support();
		switch (type[4]) {
		case '2':
			get_uints(line, uints, 2);
			glUniform2uiv(loc, 1, uints);
			return;
		case '3':
			get_uints(line, uints, 3);
			glUniform3uiv(loc, 1, uints);
			return;
		case '4':
			get_uints(line, uints, 4);
			glUniform4uiv(loc, 1, uints);
			return;
		}
	} else if (string_match("mat", type) && type[3] != '\0') {
		char cols = type[3];
		char rows = type[4] == 'x' ? type[5] : cols;
		switch (cols) {
		case '2':
			switch (rows) {
			case '2':
				get_floats(line, f, 4);
				glUniformMatrix2fv(loc, 1, GL_FALSE, f);
				return;
			case '3':
				get_floats(line, f, 6);
				glUniformMatrix2x3fv(loc, 1, GL_FALSE, f);
				return;
			case '4':
				get_floats(line, f, 8);
				glUniformMatrix2x4fv(loc, 1, GL_FALSE, f);
				return;
			}
		case '3':
			switch (rows) {
			case '2':
				get_floats(line, f, 6);
				glUniformMatrix3x2fv(loc, 1, GL_FALSE, f);
				return;
			case '3':
				get_floats(line, f, 9);
				glUniformMatrix3fv(loc, 1, GL_FALSE, f);
				return;
			case '4':
				get_floats(line, f, 12);
				glUniformMatrix3x4fv(loc, 1, GL_FALSE, f);
				return;
			}
		case '4':
			switch (rows) {
			case '2':
				get_floats(line, f, 8);
				glUniformMatrix4x2fv(loc, 1, GL_FALSE, f);
				return;
			case '3':
				get_floats(line, f, 12);
				glUniformMatrix4x3fv(loc, 1, GL_FALSE, f);
				return;
			case '4':
				get_floats(line, f, 16);
				glUniformMatrix4fv(loc, 1, GL_FALSE, f);
				return;
			}
		}
	}

	strcpy_to_space(name, type);
	printf("unknown uniform type \"%s\"", name);
	piglit_report_result(PIGLIT_FAIL);

	return;
}

void
set_parameter(const char *line)
{
	float f[4];
	int index, count;
	char type[1024];

	count = sscanf(line, "%s %d (%f , %f , %f , %f)",
		       type, &index, &f[0], &f[1], &f[2], &f[3]);
	if (count != 6) {
		fprintf(stderr, "Couldn't parse parameter command:\n%s\n", line);
		piglit_report_result(PIGLIT_FAIL);
	}

	if (string_match("env_vp", type)) {
		glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, index, f);
	} else if (string_match("local_vp", type)) {
		glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, index, f);
	} else if (string_match("env_fp", type)) {
		glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, index, f);
	} else if (string_match("local_fp", type)) {
		glProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, index, f);
	} else {
		fprintf(stderr, "Unknown parameter type `%s'\n", type);
		piglit_report_result(PIGLIT_FAIL);
	}
}

struct enable_table {
	const char *name;
	GLenum value;
} enable_table[] = {
	{ "GL_CLIP_PLANE0", GL_CLIP_PLANE0 },
	{ "GL_CLIP_PLANE1", GL_CLIP_PLANE1 },
	{ "GL_CLIP_PLANE2", GL_CLIP_PLANE2 },
	{ "GL_CLIP_PLANE3", GL_CLIP_PLANE3 },
	{ "GL_CLIP_PLANE4", GL_CLIP_PLANE4 },
	{ "GL_CLIP_PLANE5", GL_CLIP_PLANE5 },
	{ "GL_CLIP_PLANE6", GL_CLIP_PLANE0+6 },
	{ "GL_CLIP_PLANE7", GL_CLIP_PLANE0+7 },
	{ "GL_VERTEX_PROGRAM_TWO_SIDE", GL_VERTEX_PROGRAM_TWO_SIDE },
	{ NULL, 0 }
};

void
do_enable_disable(const char *line, bool enable_flag)
{
	char name[512];
	int i;

	strcpy_to_space(name, eat_whitespace(line));
	for (i = 0; enable_table[i].name; ++i) {
		if (0 == strcmp(name, enable_table[i].name)) {
			if (enable_flag) {
				glEnable(enable_table[i].value);
			} else {
				glDisable(enable_table[i].value);
			}
			return;
		}
	}

	printf("unknown enable/disable enum \"%s\"", name);
	piglit_report_result(PIGLIT_FAIL);
}

static void
draw_instanced_rect(int primcount, float x, float y, float w, float h)
{
	float verts[4][4];

	piglit_require_extension("GL_ARB_draw_instanced");

	verts[0][0] = x;
	verts[0][1] = y;
	verts[0][2] = 0.0;
	verts[0][3] = 1.0;
	verts[1][0] = x + w;
	verts[1][1] = y;
	verts[1][2] = 0.0;
	verts[1][3] = 1.0;
	verts[2][0] = x + w;
	verts[2][1] = y + h;
	verts[2][2] = 0.0;
	verts[2][3] = 1.0;
	verts[3][0] = x;
	verts[3][1] = y + h;
	verts[3][2] = 0.0;
	verts[3][3] = 1.0;

	glVertexPointer(4, GL_FLOAT, 0, verts);
	glEnableClientState(GL_VERTEX_ARRAY);

	glDrawArraysInstanced(GL_QUADS, 0, 4, primcount);

	glDisableClientState(GL_VERTEX_ARRAY);
}


struct string_to_enum {
	const char *name;
	GLenum token;
};

GLenum
decode_drawing_mode(const char *mode_str)
{
	int i;

	for (i = GL_POINTS; i <= GL_POLYGON; ++i) {
		const char *name = piglit_get_prim_name(i);
		if (0 == strcmp(mode_str, name))
			return i;
	}

	printf("unknown drawing mode \"%s\"", mode_str);
	piglit_report_result(PIGLIT_FAIL);

	/* Should not be reached, but return 0 to avoid compiler warning */
	return 0;
}

static void
handle_texparameter(const char *line)
{
	const struct string_to_enum texture_target[] = {
		{ "1D ",        GL_TEXTURE_1D             },
		{ "2D ",        GL_TEXTURE_2D             },
		{ "3D ",        GL_TEXTURE_3D             },
		{ "Rect ",      GL_TEXTURE_RECTANGLE      },
		{ "Cube ",      GL_TEXTURE_CUBE_MAP       },
		{ "1DArray ",   GL_TEXTURE_1D_ARRAY       },
		{ "2DArray ",   GL_TEXTURE_2D_ARRAY       },
		{ "CubeArray ", GL_TEXTURE_CUBE_MAP_ARRAY },
		{ NULL, 0 }
	};

	const struct string_to_enum compare_funcs[] = {
		{ "greater", GL_GREATER },
		{ "gequal", GL_GEQUAL },
		{ "less", GL_LESS },
		{ "lequal", GL_LEQUAL },
		{ "equal", GL_EQUAL },
		{ "notequal", GL_NOTEQUAL },
		{ "never", GL_NEVER },
		{ "always", GL_ALWAYS },
		{ NULL, 0 },
	};
	const struct string_to_enum depth_modes[] = {
		{ "intensity", GL_INTENSITY },
		{ "luminance", GL_LUMINANCE },
		{ "alpha", GL_ALPHA },
		{ "red", GL_RED }, /* Requires GL 3.0 or GL_ARB_texture_rg */
		{ NULL, 0 },
	};
	const struct string_to_enum min_filter_modes[] = {
		{ "nearest_mipmap_nearest", GL_NEAREST_MIPMAP_NEAREST },
		{ "linear_mipmap_nearest",  GL_LINEAR_MIPMAP_NEAREST  },
		{ "nearest_mipmap_linear",  GL_NEAREST_MIPMAP_LINEAR  },
		{ "linear_mipmap_linear",   GL_LINEAR_MIPMAP_LINEAR   },
		{ "nearest",                GL_NEAREST                },
		{ "linear",                 GL_LINEAR                 },
		{ NULL, 0 }
	};
	const struct string_to_enum mag_filter_modes[] = {
		{ "nearest",                GL_NEAREST                },
		{ "linear",                 GL_LINEAR                 },
		{ NULL, 0 }
	};
	GLenum target = 0;
	GLenum parameter;
	const char *parameter_name;
	const struct string_to_enum *strings = NULL;
	int i;

	for (i = 0; texture_target[i].name; i++) {
		if (string_match(texture_target[i].name, line)) {
			target = texture_target[i].token;
			line += strlen(texture_target[i].name);
			line = eat_whitespace(line);
			break;
		}
	}

	if (!target) {
		fprintf(stderr, "bad texture target in `texparameter %s`\n", line);
		piglit_report_result(PIGLIT_FAIL);
	}

	if (string_match("compare_func ", line)) {
		parameter = GL_TEXTURE_COMPARE_FUNC;
		parameter_name = "compare_func";
		line += strlen("compare_func ");
		strings = compare_funcs;
	} else if (string_match("depth_mode ", line)) {
		parameter = GL_DEPTH_TEXTURE_MODE;
		parameter_name = "depth_mode";
		line += strlen("depth_mode ");
		strings = depth_modes;
	} else if (string_match("min ", line)) {
		parameter = GL_TEXTURE_MIN_FILTER;
		parameter_name = "min";
		line += strlen("min ");
		strings = min_filter_modes;
	} else if (string_match("mag ", line)) {
		parameter = GL_TEXTURE_MAG_FILTER;
		parameter_name = "mag";
		line += strlen("mag ");
		strings = mag_filter_modes;
	} else if (string_match("lod_bias ", line)) {
#ifdef PIGLIT_USE_OPENGL
		line += strlen("lod_bias ");
		glTexParameterf(target, GL_TEXTURE_LOD_BIAS,
				strtod(line, NULL));
		return;
#else
		printf("lod_bias feature is only available in desktop GL\n");
		piglit_report_result(PIGLIT_SKIP);
#endif
	} else {
		fprintf(stderr, "unknown texture parameter in `%s'\n", line);
		piglit_report_result(PIGLIT_FAIL);
	}

	for (i = 0; strings[i].name; i++) {
		if (string_match(strings[i].name, line)) {
			glTexParameteri(target, parameter, strings[i].token);
			return;
		}
	}

	fprintf(stderr, "Bad %s `%s'\n", parameter_name, line);
	piglit_report_result(PIGLIT_FAIL);
}

static void
setup_ubos()
{
	int i;

	if (!piglit_is_extension_supported("GL_ARB_uniform_buffer_object") &&
	    piglit_get_gl_version() < 31) {
		return;
	}

	glGetProgramiv(prog, GL_ACTIVE_UNIFORM_BLOCKS, &num_uniform_blocks);
	if (num_uniform_blocks == 0)
		return;

	uniform_block_bos = calloc(num_uniform_blocks, sizeof(GLuint));
	glGenBuffers(num_uniform_blocks, uniform_block_bos);

	for (i = 0; i < num_uniform_blocks; i++) {
		GLint size;

		glGetActiveUniformBlockiv(prog, i, GL_UNIFORM_BLOCK_DATA_SIZE,
					  &size);

		glBindBuffer(GL_UNIFORM_BUFFER, uniform_block_bos[i]);
		glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, i, uniform_block_bos[i]);
	}
}

void
program_must_be_in_use(void)
{
	if (!link_ok) {
		fprintf(stderr, "Failed to link:\n%s\n", prog_err_info);
		piglit_report_result(PIGLIT_FAIL);
	} else if (!prog_in_use) {
		fprintf(stderr, "Failed to use program: %s\n", prog_err_info);
		piglit_report_result(PIGLIT_FAIL);
	}

}

enum piglit_result
piglit_display(void)
{
	const char *line;
	bool pass = true;
	GLbitfield clear_bits = 0;
	bool link_error_expected = false;

	if (test_start == NULL)
		return PIGLIT_PASS;

	line = test_start;
	while (line[0] != '\0') {
		float c[32];
		double d[4];
		int x, y, w, h, l, tex, level;
		char s[32];

		line = eat_whitespace(line);

		if (string_match("clear color", line)) {
			get_floats(line + 11, c, 4);
			glClearColor(c[0], c[1], c[2], c[3]);
			clear_bits |= GL_COLOR_BUFFER_BIT;
		} else if (string_match("clear", line)) {
			glClear(clear_bits);
		} else if (sscanf(line,
				  "clip plane %d %lf %lf %lf %lf",
				  &x, &d[0], &d[1], &d[2], &d[3])) {
			if (x < 0 || x >= GL_MAX_CLIP_PLANES) {
				printf("clip plane id %d out of range", x);
				piglit_report_result(PIGLIT_FAIL);
			}
			glClipPlane(GL_CLIP_PLANE0 + x, d);
		} else if (string_match("draw rect tex", line)) {
			program_must_be_in_use();
			get_floats(line + 13, c, 8);
			piglit_draw_rect_tex(c[0], c[1], c[2], c[3],
					     c[4], c[5], c[6], c[7]);
		} else if (string_match("draw rect", line)) {
			program_must_be_in_use();
			get_floats(line + 9, c, 4);
			piglit_draw_rect(c[0], c[1], c[2], c[3]);
		} else if (string_match("draw instanced rect", line)) {
			int primcount;

			program_must_be_in_use();
			sscanf(line + 19, "%d %f %f %f %f",
			       &primcount,
			       c + 0, c + 1, c + 2, c + 3);
			draw_instanced_rect(primcount, c[0], c[1], c[2], c[3]);
		} else if (sscanf(line, "draw arrays %31s %d %d", s, &x, &y)) {
			GLenum mode = decode_drawing_mode(s);
			int first = x;
			size_t count = (size_t) y;
			program_must_be_in_use();
			if (first < 0) {
				printf("draw arrays 'first' must be >= 0\n");
				piglit_report_result(PIGLIT_FAIL);
			} else if ((size_t) first >= num_vbo_rows) {
				printf("draw arrays 'first' must be < %lu\n",
				       (unsigned long) num_vbo_rows);
				piglit_report_result(PIGLIT_FAIL);
			}
			if (count <= 0) {
				printf("draw arrays 'count' must be > 0\n");
				piglit_report_result(PIGLIT_FAIL);
			} else if (count > num_vbo_rows - (size_t) first) {
				printf("draw arrays cannot draw beyond %lu\n",
				       (unsigned long) num_vbo_rows);
				piglit_report_result(PIGLIT_FAIL);
			}
			/* TODO: wrapper? */
			glDrawArrays(mode, first, count);
		} else if (string_match("disable", line)) {
			do_enable_disable(line + 7, false);
		} else if (string_match("enable", line)) {
			do_enable_disable(line + 6, true);
		} else if (string_match("frustum", line)) {
			get_floats(line + 7, c, 6);
			piglit_frustum_projection(false, c[0], c[1], c[2],
						  c[3], c[4], c[5]);
		} else if (sscanf(line, "ortho %f %f %f %f",
				  c + 0, c + 1, c + 2, c + 3) == 4) {
			piglit_gen_ortho_projection(c[0], c[1], c[2], c[3],
						    -1, 1, GL_FALSE);
		} else if (string_match("ortho", line)) {
			piglit_ortho_projection(piglit_width, piglit_height,
						GL_FALSE);
		} else if (string_match("probe rgba", line)) {
			get_floats(line + 10, c, 6);
			if (!piglit_probe_pixel_rgba((int) c[0], (int) c[1],
						    & c[2])) {
				pass = false;
			}
		} else if (sscanf(line,
				  "relative probe rgba ( %f , %f ) "
				  "( %f , %f , %f , %f )",
				  c + 0, c + 1,
				  c + 2, c + 3, c + 4, c + 5) == 6) {
			x = c[0] * piglit_width;
			y = c[1] * piglit_width;
			if (x >= piglit_width)
				x = piglit_width - 1;
			if (y >= piglit_height)
				y = piglit_height - 1;

			if (!piglit_probe_pixel_rgba(x, y, &c[2])) {
				pass = false;
			}
		} else if (string_match("probe rgb", line)) {
			get_floats(line + 9, c, 5);
			if (!piglit_probe_pixel_rgb((int) c[0], (int) c[1],
						    & c[2])) {
				pass = false;
			}
		} else if (sscanf(line,
				  "relative probe rgb ( %f , %f ) "
				  "( %f , %f , %f )",
				  c + 0, c + 1,
				  c + 2, c + 3, c + 4) == 5) {
			x = c[0] * piglit_width;
			y = c[1] * piglit_width;
			if (x >= piglit_width)
				x = piglit_width - 1;
			if (y >= piglit_height)
				y = piglit_height - 1;

			if (!piglit_probe_pixel_rgb(x, y, &c[2])) {
				pass = false;
			}
		} else if (string_match("probe all rgba", line)) {
			get_floats(line + 14, c, 4);
			pass = pass &&
				piglit_probe_rect_rgba(0, 0, piglit_width,
						       piglit_height, c);
		} else if (string_match("probe all rgb", line)) {
			get_floats(line + 13, c, 3);
			pass = pass &&
				piglit_probe_rect_rgb(0, 0, piglit_width,
						      piglit_height, c);
		} else if (string_match("tolerance", line)) {
			get_floats(line + strlen("tolerance"), piglit_tolerance, 4);
		} else if (string_match("shade model smooth", line)) {
			glShadeModel(GL_SMOOTH);
		} else if (string_match("shade model flat", line)) {
			glShadeModel(GL_FLAT);
		} else if (sscanf(line,
				  "texture rgbw %d ( %d , %d )",
				  &tex, &w, &h) == 3) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_rgbw_texture(GL_RGBA, w, h, GL_FALSE, GL_FALSE, GL_UNSIGNED_NORMALIZED);
			glEnable(GL_TEXTURE_2D);
		} else if (sscanf(line, "texture miptree %d", &tex) == 1) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_miptree_texture();
			glEnable(GL_TEXTURE_2D);
		} else if (sscanf(line,
				  "texture checkerboard %d %d ( %d , %d ) "
				  "( %f , %f , %f , %f ) "
				  "( %f , %f , %f , %f )",
				  &tex, &level, &w, &h,
				  c + 0, c + 1, c + 2, c + 3,
				  c + 4, c + 5, c + 6, c + 7) == 12) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_checkerboard_texture(0, level,
						    w, h,
						    w / 2, h / 2,
						    c + 0, c + 4);
			glEnable(GL_TEXTURE_2D);
		} else if (sscanf(line,
				  "texture shadow2D %d ( %d , %d )",
				  &tex, &w, &h) == 3) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_depth_texture(GL_TEXTURE_2D, GL_DEPTH_COMPONENT,
					     w, h, 1, GL_FALSE);
			glTexParameteri(GL_TEXTURE_2D,
					GL_TEXTURE_COMPARE_MODE,
					GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D,
					GL_TEXTURE_COMPARE_FUNC,
					GL_GREATER);
			glTexParameteri(GL_TEXTURE_2D,
					GL_DEPTH_TEXTURE_MODE,
					GL_INTENSITY);

			glEnable(GL_TEXTURE_2D);
		} else if (sscanf(line,
				  "texture shadowRect %d ( %d , %d )",
				  &tex, &w, &h) == 3) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_depth_texture(GL_TEXTURE_RECTANGLE, GL_DEPTH_COMPONENT,
					     w, h, 1, GL_FALSE);
			glTexParameteri(GL_TEXTURE_RECTANGLE,
					GL_TEXTURE_COMPARE_MODE,
					GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_RECTANGLE,
					GL_TEXTURE_COMPARE_FUNC,
					GL_GREATER);
			glTexParameteri(GL_TEXTURE_RECTANGLE,
					GL_DEPTH_TEXTURE_MODE,
					GL_INTENSITY);
		} else if (sscanf(line,
				  "texture shadow1D %d ( %d )",
				  &tex, &w) == 2) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_depth_texture(GL_TEXTURE_1D, GL_DEPTH_COMPONENT,
					     w, 1, 1, GL_FALSE);
			glTexParameteri(GL_TEXTURE_1D,
					GL_TEXTURE_COMPARE_MODE,
					GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_1D,
					GL_TEXTURE_COMPARE_FUNC,
					GL_GREATER);
			glTexParameteri(GL_TEXTURE_1D,
					GL_DEPTH_TEXTURE_MODE,
					GL_INTENSITY);
		} else if (sscanf(line,
				  "texture shadow1DArray %d ( %d , %d )",
				  &tex, &w, &l) == 3) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_depth_texture(GL_TEXTURE_1D_ARRAY, GL_DEPTH_COMPONENT,
					     w, 1, l, GL_FALSE);
			glTexParameteri(GL_TEXTURE_1D_ARRAY,
					GL_TEXTURE_COMPARE_MODE,
					GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_1D_ARRAY,
					GL_TEXTURE_COMPARE_FUNC,
					GL_GREATER);
			glTexParameteri(GL_TEXTURE_1D_ARRAY,
					GL_DEPTH_TEXTURE_MODE,
					GL_INTENSITY);
		} else if (sscanf(line,
				  "texture shadow2DArray %d ( %d , %d , %d )",
				  &tex, &w, &h, &l) == 4) {
			glActiveTexture(GL_TEXTURE0 + tex);
			piglit_depth_texture(GL_TEXTURE_2D_ARRAY, GL_DEPTH_COMPONENT,
					     w, h, l, GL_FALSE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY,
					GL_TEXTURE_COMPARE_MODE,
					GL_COMPARE_R_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY,
					GL_TEXTURE_COMPARE_FUNC,
					GL_GREATER);
			glTexParameteri(GL_TEXTURE_2D_ARRAY,
					GL_DEPTH_TEXTURE_MODE,
					GL_INTENSITY);
		} else if (string_match("texparameter ", line)) {
			handle_texparameter(line + strlen("texparameter "));
		} else if (string_match("uniform", line)) {
			program_must_be_in_use();
			set_uniform(line + 7);
		} else if (string_match("parameter ", line)) {
			set_parameter(line + strlen("parameter "));
		} else if (string_match("link error", line)) {
			link_error_expected = true;
			if (link_ok) {
				printf("shader link error expected, but it was sucessful!\n");
				piglit_report_result(PIGLIT_FAIL);
			}
		} else if (string_match("link success", line)) {
			program_must_be_in_use();
		} else if ((line[0] != '\n') && (line[0] != '\0')
			   && (line[0] != '#')) {
			printf("unknown command \"%s\"", line);
			piglit_report_result(PIGLIT_FAIL);
		}

		line = strchrnul(line, '\n');
		if (line[0] != '\0')
			line++;
	}

	if (!link_ok && !link_error_expected) {
		program_must_be_in_use();
	}

	piglit_present_results();

	if (piglit_automatic) {
		/* Free our resources, useful for valgrinding. */
		glDeleteProgram(prog);
		glUseProgram(0);
	}

	return pass ? PIGLIT_PASS : PIGLIT_FAIL;
}


void
piglit_init(int argc, char **argv)
{
	int major;
	int minor;
	bool es;

	piglit_require_GLSL();

	version_init(&gl_version, VERSION_GL,
	             piglit_is_gles(),
	             piglit_get_gl_version());

	piglit_get_glsl_version(&es, &major, &minor);
	version_init(&glsl_version, VERSION_GLSL, es,
	             (major * 100) + minor);

	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
		      &gl_max_fragment_uniform_components);
	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
		      &gl_max_vertex_uniform_components);

	if (argc > 2) {
		path = argv[2];
	} else if (argc > 1) {
#if defined(_WIN32)
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		char* scriptpath;
		_splitpath(argv[1], drive, dir, fname, ext);
		scriptpath = malloc(strlen(drive) + strlen(dir) + 1);
		strcpy(scriptpath, drive);
		strcat(scriptpath, dir);
		path = scriptpath;
#else
		/* Because dirname()'s memory handling is unpredictable, we
		 * must copy both its input and ouput. */
		char* scriptpath = strdup(argv[1]);
		path = strdup(dirname(scriptpath));
		free(scriptpath);
#endif
	} else {
		printf("shader_runner: missing arguments\n");
		exit(1);
	}

	process_test_script(argv[1]);
	link_and_use_shaders();
	if (vertex_data_start != NULL) {
		if (gl_version.num >= 31) {
			GLuint vao;

			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
		}

		num_vbo_rows = setup_vbo_from_text(prog, vertex_data_start,
						   vertex_data_end);
	}
	setup_ubos();
}
