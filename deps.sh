#!/bin/bash
set -e

fetch(){
	NAME=$1;VERSION=$2;URL=$3;CONF_FLAGS=$4;MAKE_FLAGS=$5;
	LIBNAME=lib${NAME/lib/}
	VITA_FLAGS="${VITA_FLAGS} ${LIBRARY_PATH}/${LIBNAME}.a -I${LIBRARY_PATH}/${LIBNAME}/include"
	echo "Fetch/Build/Install ${NAME}-${VERSION}..."
	if [ ! -f ${LIBRARY_PATH}/${LIBNAME}.a ] ;then # no .a found, build it
		curl -f -s -L ${URL}/${NAME}-${VERSION}.tar.gz | tar xz -C ${SOURCEDIR}
		cd ${SOURCEDIR}/${NAME}-${VERSION}
		${SOURCEDIR}/${NAME}-${VERSION}/configure --prefix=${DEPSDIR} ${CONF_FLAGS}
		make ${MAKE_FLAGS} install
	fi
}
build(){
	echo "Building $1..."
	sources=""
	for src in $@ ; do sources="${sources} src/${src}.c" ; done
	gcc $sources -o $1 $VITA_FLAGS
}

CWD=$PWD
DEPSDIR=$PWD/deps
SOURCEDIR=$DEPSDIR/src
mkdir -p ${SOURCEDIR}
mkdir -p ${DEPSDIR}
BINARY_PATH=${DEPSDIR}/bin
INCLUDE_PATH=${DEPSDIR}/include
LIBRARY_PATH=${DEPSDIR}/lib
VITA_FLAGS="-I${INCLUDE_PATH} -w -std=gnu99"

fetch libelf 0.8.13 www.mr511.de/software       ""
fetch libzip 1.1.3  nih.at/libzip               "--disable-shared --enable-static CFLAGS=-DZIP_STATIC"
fetch yaml   0.1.7  pyyaml.org/download/libyaml "--disable-shared --enable-static CFLAGS=-DYAML_DECLARE_STATIC"
fetch zlib   1.2.8  zlib.net                    "--static"

# Cmake build alternative
#cd $CWD
# build vita-elf-create velf import import-parse export-parse elf-defs sce-elf varray elf-utils sha256 yaml-tree yaml-treeutil
# build vita-elf-export yaml-tree yaml-treeutil sha256 yaml-emitter export-parse
# build vita-libs-gen import import-parse yaml-tree yaml-treeutil
# build vita-mksfoex
# build vita-make-sfo
# build vita-make-fself sha256
# build vita-make-vpk
# build vita-pack-vpk
