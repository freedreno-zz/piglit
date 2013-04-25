#!/bin/bash

function emit_vs
{
    targetdir=$1
    t=$2

    echo "$targetdir/outerProduct-$t.vert"
    cat > $targetdir/outerProduct-$t.vert <<EOF
/* [config]
 * expect_result: fail
 * glsl_version: 3.00
 * [end config]
 * outerProduct is available in OpenGL SL ES 3.00 but
 * not for all data types.
 *
 * http://www.khronos.org/opengles/sdk/docs/manglsl/xhtml/outerProduct.xml
 */
void main () {
  gl_Position = vec4(0);
  outerProduct(${t}(0), ${t}(0));
}
EOF
}

function emit_valid_vs
{
    targetdir=$1
    t=$2

    echo "$targetdir/outerProduct-$t.vert"
    cat > $targetdir/outerProduct-$t.vert <<EOF
/* [config]
 * expect_result: pass
 * glsl_version: 3.00
 * [end config]
 * outerProduct is available in OpenGL SL ES 3.00.
 * not for all data types.
 *
 * http://www.khronos.org/opengles/sdk/docs/manglsl/xhtml/outerProduct.xml
 */
void main () {
  gl_Position = vec4(0);
  outerProduct(${t}(0), ${t}(0));
}
EOF
}

function emit_valid_mixed_vs
{
    targetdir=$1
    t=$2
    u=$3

    echo "$targetdir/outerProduct-$t.vert"
    cat > $targetdir/outerProduct-$t.vert <<EOF
/* [config]
 * expect_result: pass
 * glsl_version: 3.00
 * [end config]
 * outerProduct is available in OpenGL SL ES 3.00.
 * not for all data types.
 *
 * http://www.khronos.org/opengles/sdk/docs/manglsl/xhtml/outerProduct.xml
 */
void main () {
  gl_Position = vec4(0);
  outerProduct(${t}(0), ${u}(0));
}
EOF
}

set +x

targetdir="spec/glsl-es-3.00/compiler/built-in-functions"
mkdir -p $targetdir

for i in int float bool bvec2 bvec3 bvec4 mat2 mat2x2 mat2x3 mat2x4 mat3 mat3x2 mat3x3 mat3x4 mat4 mat4x2 mat4x3 mat4x4
do
    emit_vs $targetdir $i
done
for i in vec2 vec3 vec4
do
    emit_valid_vs $targetdir $i
done
emit_valid_mixed_vs $targetdir vec3 vec2
emit_valid_mixed_vs $targetdir vec2 vec3
emit_valid_mixed_vs $targetdir vec4 vec2
emit_valid_mixed_vs $targetdir vec4 vec3
emit_valid_mixed_vs $targetdir vec2 vec4
emit_valid_mixed_vs $targetdir vec3 vec4
