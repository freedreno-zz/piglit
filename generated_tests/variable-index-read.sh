#!/bin/bash

# Generate a set of data for a sized matrix.  Elements start with a specified
# value and increment by 1.0 for each element.
function matrix_data
{
    first=$1
    dim=$2

    last=$(($first + $dim * $dim - 1))
    seq $first $last | tr '\n' ' ' | sed 's/ /.0, /g;s/, $//;s/ *$//g'
}


function emit_matrix_array_initializer
{
    matrix_dim=$1
    array_dim=$2
    base_type=$3
    indent=$4

    for c in $(seq $array_dim); do
	if [ $c -ne 1 ]; then
	    echo ","
	fi

	first=$(((c - 1) * matrix_dim * matrix_dim + 1))
	echo -n "${indent}    ${base_type}("
	matrix_data $first $matrix_dim
	echo -n ")"
    done

}


# Emit global variable declarations for either the vertex shader or the
# fragment shader.
function emit_globals
{
    matrix_dim=$1
    array_dim=$2
    mode=$3
    index_value=$4
    col=$5
    expect_type=$6

    v=${version/./}
    if [ $v -ge 120 ]; then
	base_type="mat${matrix_dim}x${matrix_dim}"
    else
	base_type="mat${matrix_dim}"
    fi

    type=$base_type
    dim=""
    if [ $array_dim -ne 0 ]; then
	if [ $v -ge 120 ]; then
	    type="${type}[$array_dim]"
	else
	    dim="[${array_dim}]"
	fi
    fi

    if [ $array_dim -ne 0 -a "x$index_value" = "xindex" ]; then
	echo "uniform int index;"
    fi

    if [ "x$col" = "xcol" ]; then
	echo "uniform int col;"
    fi

    if [ "x$expect_type" == "xfloat" ]; then
	echo "uniform int row;"
    fi

    echo "uniform ${expect_type} expect;"

    if [ $v -ge 120 -a "x$mode" = "xuniform" ]; then
	if [ $array_dim -eq 0 ]; then
            echo -n "${mode} ${type} m = ${type}("
	    matrix_data 1 $matrix_dim
	    echo ");"
	else
	    echo "${mode} ${type} m${dim} = ${type}("
	    emit_matrix_array_initializer $matrix_dim $array_dim $base_type ""
	    echo ");"
	fi
    elif [ "x$mode" != "xtemp" ]; then
        echo "${mode} ${type} m${dim};"
    fi
    echo "varying vec4 color;"
    echo
}


function emit_set_matrix
{
    matrix_dim=$1
    array_dim=$2
    mode=$3

    v=${version/./}
    if [ $v -ge 120 ]; then
	base_type="mat${matrix_dim}x${matrix_dim}"
    else
	base_type="mat${matrix_dim}"
    fi

    type=$base_type
    dim=""
    if [ $array_dim -ne 0 ]; then
	if [ $v -ge 120 ]; then
	    type="${type}[$array_dim]"
	else
	    dim="[${array_dim}]"
	fi
    fi

    if [ $array_dim -ne 0 ]; then
	if [ "x$mode" = "xtemp" ]; then
	    if [ $v -ge 120 ]; then
		echo "    ${type} m${dim} = ${type}("
		emit_matrix_array_initializer $matrix_dim $array_dim $base_type "    "
		echo ");"
	    else
		echo "    ${type} m${dim};"
	    fi
	fi

	if [ $v -lt 120 -o "x$mode" = "xvarying" ]; then
	    for i in $(seq 0 $(($array_dim - 1))); do
		first=$((1 + $i * $matrix_dim * $matrix_dim))
		echo -n "    m[$i] = mat${matrix_dim}("
		matrix_data $first $matrix_dim
		echo ");"
	    done
	fi
    else
	matrix_values=$(matrix_data 1 $matrix_dim)

	if [ "x$mode" = "xtemp" ] ; then
	    echo "    ${type} m = ${type}(${matrix_values});"
	else
	    echo "    m = ${type}(${matrix_values});"
	fi
    fi
}


function emit_vs
{
    matrix_dim=$1
    array_dim=$2
    mode=$3
    index_value=$4
    col=$5
    expect_type=$6
    do_compare=$7
    v=${version/./}

    if [ $array_dim -ne 0 ]; then
	idx="[${index_value}]"
    else
	idx=""
    fi

    echo "[vertex shader]"
    if [ $v -eq 100 ]; then
        echo "attribute vec4 vertex;"
        echo "mat4 projection = mat4("
        echo "    2.0/250.0, 0.0, 0.0, -1.0,"
        echo "    0.0, 2.0/250.0, 0.0, -1.0,"
        echo "    0.0, 0.0, -1.0, 0.0,"
        echo "    0.0, 0.0, 0.0, 1.0);"
    fi
    emit_globals $*

    echo "void main()"
    echo "{"
    if [ $v -eq 100 ]; then
        echo "    gl_Position = vertex;"
        echo "    gl_Position *= projection;"
    else
        echo "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
    fi

    # Only emit the code to set the matrix if the vertex shader is generating
    # varyings for a fragment shader or the matrix is in local storage and the
    # vertex shader is doing the comparison.
    if [ "x$mode" = "xvarying" -o \( "x$mode" = "xtemp" -a  $do_compare -ne 0 \) ]; then
	echo
	emit_set_matrix $matrix_dim $array_dim $mode
    fi

    if [ $do_compare -ne 0 ]; then
        # Many people thing that a vertex shader cannot read a varying.  Quote
        # some bits from the GLSL spec so that people won't get the wrong idea
        # when reading the test.
	if [ "x$mode" = "xvarying" ]; then
	    cat <<EOF

    /* From page 23 (page 30 of the PDF) of the GLSL 1.10 spec:
     *
     *     "A vertex shader may also read varying variables, getting back the
     *     same values it has written. Reading a varying variable in a vertex
     *     shader returns undefined values if it is read before being
     *     written."
     */
EOF
	fi

	if [ "x$expect_type" = "xfloat" ]; then
	    echo "    color = (m${idx}[$col][row] == expect)"
	else
	    echo "    color = (m${idx}[$col] == expect)"
	fi
	echo "        ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);"
    fi
    echo -e "}\n"
}


emit_fs()
{
    matrix_dim=$1
    array_dim=$2
    mode=$3
    index_value=$4
    col=$5
    expect_type=$6
    do_compare=$7
    v=${version/./}

    echo "[fragment shader]"
    if [ $v -eq 100 ]; then
        echo "precision highp float;"
        echo "precision highp int;"
    fi
    emit_globals $*

    echo "void main()"
    echo "{"

    if [ $do_compare -eq 0 -a "x$mode" = "xvarying" ]; then
	cat <<EOF
    /* There is some trickery here.  The fragment shader has to actually use
     * the varyings generated by the vertex shader, or the compiler (more
     * likely the linker) might demote the varying outputs to just be vertex
     * shader global variables.  Since the point of the test is the vertex
     * shader reading from a varying, that would defeat the test.
     */
EOF
    fi

    if [ $do_compare -ne 0 -o "x$mode" = "xvarying" ]; then
	if [ "x$mode" = "xtemp" ]; then
	    emit_set_matrix $matrix_dim $array_dim $mode
	    echo
	fi

	if [ $array_dim -ne 0 ]; then
	    idx="[$index_value]"
	else
	    idx=""
	fi

	if [ "x$expect_type" = "xfloat" ]; then
	    row="[row]"
	else
	    row=""
	fi

	echo "    gl_FragColor = (m${idx}[${col}]${row} == expect)"

	if [ $do_compare -eq 0 ]; then
            echo "        ? color : vec4(1.0, 0.0, 0.0, 1.0);"
	else
            echo "        ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);"
	fi
    else
	echo "    gl_FragColor = color;"
    fi

    echo "}"
    echo
}


function emit_test_vectors
{
    matrix_dim=$1
    local array_dim=$2
    mode=$3
    index_value=$4
    col=$5
    expect_type=$6
    v=${version/./}

    # Optimizing GLSL linkers may reduce the size of the uniform array if tail
    # elements are not accessed.  Shader runner will fail the test if one of
    # the set uniforms doesn't have a location.
    if [ "x$mode" = "xuniform" -a $v -le 110 -a $array_dim -ne 0 -a "x$index_value" != "xindex" ]; then
	array_dim=$((index_value+1))
    fi

    if [ "$v" -eq 100 ]; then
        cat <<EOF
[test]
clear color 0.5 0.5 0.5 0.5
clear

EOF
    else
        cat <<EOF
[test]
clear color 0.5 0.5 0.5 0.5
clear
ortho

EOF
    fi

    # NOTE: shader_runner uses the matCxR names even for GLSL 1.10
    type="mat${matrix_dim}x${matrix_dim}"
    if [ "x$mode" = "xuniform" -a $v -le 110 ]; then
	if [ $array_dim -eq 0 ]; then
	    echo -n "uniform ${type} m "
	    matrix_data 1 $matrix_dim | sed 's/,//g'
	    echo
	fi
    fi

    if [ $array_dim -eq 0 ]; then
	sizes="1"
    elif [ "x$index_value" = "xindex" ]; then
	sizes=$(seq $array_dim)
    else
	sizes="2"
    fi

    if [ "x$col" = "xcol" ]; then
	columns=$(seq $matrix_dim)
    else
	columns=2
    fi

    if [ "x$expect_type" = "xfloat" ]; then
	rows=$(seq $matrix_dim)
    else
	rows=1
    fi

    for i in $sizes; do
	if [ "x$mode" = "xuniform" -a $v -le 110 ]; then
	    if [ $array_dim -ne 0 ]; then
		for c in $(seq 0 $(($array_dim - 1))); do
		    first=$((1 + c * matrix_dim * matrix_dim))
		    echo -n "uniform ${type} m[$c] "
		    matrix_data $first $matrix_dim | sed 's/,//g'
		    echo
		done
	    fi
	fi

	if [ $array_dim -ne 0 -a "x$index_value" = "xindex" ]; then
	    echo "uniform int index $((i - 1))"
	fi

	x_base=$(((i - 1) * (15 * matrix_dim + 10)))
	for c in $columns; do
	    if [ "x$col" = "xcol" ]; then
		echo "uniform int col $((c - 1))"
	    fi

	    for r in $rows; do
		expect=$(((i - 1) * (matrix_dim * matrix_dim) + (c - 1) * matrix_dim + r))
		if [ "x$expect_type" = "xfloat" ]; then
		    echo "uniform int row $((r - 1))"
		    echo "uniform float expect $expect"
		else
		    e=$(seq $expect $((expect + matrix_dim - 1)) | tr '\n' ' ' | sed 's/[[:space:]]*$//g')
		    echo "uniform ${expect_type} expect $e"
		fi

		x=$((x_base + 15 * c - 10))
		y=$((15 * r - 10))
		echo "draw rect $x $y 10 10"

		x=$((x + 5))
		y=$((y + 5))
		echo "probe rgb $x $y 0.0 1.0 0.0"
		echo
	    done
	done
    done
}

# Generate a test that read from an (array of) matrix using a non-constant
# index in the fragment shader.
function emit_fs_rd_test
{
    v=${version/./}

    echo "# Test generated by:"
    echo "# ${cmd}"
    echo
    echo "[require]"
    if [ $v -eq 100 ]; then
        echo "GLSL ES >= $version"
        echo "GL ES >= 2.0"
    else
        echo "GLSL >= $version"
    fi
    echo

    emit_vs $* 0
    emit_fs $* 1

    emit_test_vectors $*
}


# Generate a test that read from an (array of) matrix using a non-constant
# index in the fragment shader.
function emit_vs_rd_test
{
    v=${version/./}

    echo "# Test generated by:"
    echo "# ${cmd}"
    echo
    echo "[require]"
    if [ $v -eq 100 ]; then
        echo "GLSL ES >= $version"
        echo "GL ES >= 2.0"
    else
        echo "GLSL >= $version"
    fi
    echo

    emit_vs $* 1
    emit_fs $* 0

    emit_test_vectors $*
}

cmd="$0 $*"

if [ "x$1" = "x" ]; then
    version="1.10"
else
    case "$1" in
	1.[012]0) version="$1";;
	*)
	    echo "Bogus GLSL version \"$1\" specified."
	    exit 1
	    ;;
    esac
fi

v=${version/./}
if [ $v -eq 100 ]; then
	es="-es"
fi

filepath="spec/glsl"${es}-${version}"/execution/varible-index-read"
mkdir -p ${filepath}

for mode in temp uniform varying; do
    # More than 3 is unlikely to work for the varying tests due to using too
    # many varying vectors.  mat4[3] uses 12 varying vectors by itself.
    for array_dim in 0 3; do
	for matrix_dim in 2 3 4; do
	    if [ $array_dim -ne 0 ]; then
		arr="array-"
		idx_txt="index-"

		emit_fs_rd_test $matrix_dim $array_dim $mode 1 col float \
		    > ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-col-row-rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-col-row-rd.shader_test"

		emit_fs_rd_test $matrix_dim $array_dim $mode 1 1   float \
		    > ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-row-rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-row-rd.shader_test"

		emit_fs_rd_test $matrix_dim $array_dim $mode 1 col vec${matrix_dim} \
		    > ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-col-rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-col-rd.shader_test"

		emit_fs_rd_test $matrix_dim $array_dim $mode 1 1   vec${matrix_dim} \
		    > ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-rd.shader_test"

		emit_vs_rd_test $matrix_dim $array_dim $mode 1 col float \
		    > ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-col-row-rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-col-row-rd.shader_test"

		emit_vs_rd_test $matrix_dim $array_dim $mode 1 1   float \
		    > ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-row-rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-row-rd.shader_test"

		emit_vs_rd_test $matrix_dim $array_dim $mode 1 col vec${matrix_dim} \
		    > ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-col-rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-col-rd.shader_test"

		emit_vs_rd_test $matrix_dim $array_dim $mode 1 1   vec${matrix_dim} \
		    > ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-rd.shader_test"

	    else
		arr=""
		idx_txt=""
	    fi

	    emit_fs_rd_test $matrix_dim $array_dim $mode index col float \
		> ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-row-rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-row-rd.shader_test"

	    emit_fs_rd_test $matrix_dim $array_dim $mode index 1   float \
		> ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}row-rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}row-rd.shader_test"

	    emit_fs_rd_test $matrix_dim $array_dim $mode index col vec${matrix_dim} \
		> ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-rd.shader_test"

	    emit_fs_rd_test $matrix_dim $array_dim $mode index 1   vec${matrix_dim} \
		> ${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}rd.shader_test
		echo "${filepath}/fs-${mode}-${arr}mat${matrix_dim}-${idx_txt}rd.shader_test"

	    emit_vs_rd_test $matrix_dim $array_dim $mode index col float \
		> ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-row-rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-row-rd.shader_test"

	    emit_vs_rd_test $matrix_dim $array_dim $mode index 1   float \
		> ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}row-rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}row-rd.shader_test"

	    emit_vs_rd_test $matrix_dim $array_dim $mode index col vec${matrix_dim} \
		> ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}col-rd.shader_test"

	    emit_vs_rd_test $matrix_dim $array_dim $mode index 1   vec${matrix_dim} \
		> ${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}rd.shader_test
		echo "${filepath}/vs-${mode}-${arr}mat${matrix_dim}-${idx_txt}rd.shader_test"
	done
    done
done
