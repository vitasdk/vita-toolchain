#!/bin/bash
set -e

CWD=$PWD
DEPSDIR=$PWD/deps
SOURCEDIR=$DEPSDIR/src
PATCHDIR=$PWD/buildscripts/patches
CMAKE_FLAGS="-DCMAKE_C_FLAGS_RELEASE:STRING='-DZIP_STATIC'"

JOBS=-j2
uname_string=`uname | sed 'y/LINUXDARWINFREEOPENPCBSDMSYS/linuxdarwinfreeopenpcbsdmsys/'`
host_arch=`uname -m | sed 'y/XI/xi/'`
case "$uname_string" in
  *linux*)
    HOST_NATIVE="$host_arch"-linux-gnu
    JOBS="-j`grep ^processor /proc/cpuinfo|wc -l`"
    ;;
  *freebsd*)
    HOST_NATIVE="$host_arch"-freebsd
    JOBS="-j`sysctl kern.smp.cpus | sed 's/kern.smp.cpus: //'`"
    ;;
  *darwin*)
    HOST_NATIVE=x86_64-apple-darwin10
    JOBS="-j1"
    ;;
  *msys*)
    HOST_NATIVE="$host_arch"-w64-mingw32
    JOBS="-j4"
    ;;
  *)
    echo "Unsupported build system : `uname`"
    exit 1
    ;;
esac

fetch(){
	LIBNAME=$1;NAME=$2;VERSION=$3;URL=$4;CONF_FLAGS=$5;MAKE_FLAGS=$6;

	echo "Fetch/Build/Install ${NAME}-${VERSION}..."
	
	CMAKE_FLAGS="${CMAKE_FLAGS}\
		-D${LIBNAME}_INCLUDE_DIRS=${INCLUDE_PATH}\
		-D${LIBNAME}_LIBRARY=${LIBRARY_PATH}/${LIBNAME}.a\
		-D${LIBNAME}_CONFIG_INCLUDE_DIR=${LIBRARY_PATH}/${LIBNAME}/include"

	if [ ! -f ${LIBRARY_PATH}/${LIBNAME}.a ] ;then # no .a found, build it
		curl -f -s -L ${URL}/${NAME}-${VERSION}.tar.gz | tar xz -C ${SOURCEDIR}
		cd ${SOURCEDIR}/${NAME}-${VERSION}
		if [ -f ${PATCHDIR}/${NAME}.patch ] ; then
			patch -f < ${PATCHDIR}/${NAME}.patch || : # FIXME: silently fail on libzip
		fi
		${SOURCEDIR}/${NAME}-${VERSION}/configure --prefix=${DEPSDIR} ${CONF_FLAGS}
		make ${JOBS} ${MAKE_FLAGS} install
	fi
}

prepare(){
	if [ ! -d ${PATCHDIR} ] ; then
		git clone https://github.com/vitasdk/buildscripts
	fi
	mkdir -p ${SOURCEDIR}
	mkdir -p ${DEPSDIR}
	BINARY_PATH=${DEPSDIR}/bin
	INCLUDE_PATH=${DEPSDIR}/include
	LIBRARY_PATH=${DEPSDIR}/lib 

	fetch libz    zlib   1.2.8  zlib.net                    "--static" "" # no need for -C lib ?
	fetch libelf  libelf 0.8.13 www.mr511.de/software       "--host=${HOST_NATIVE}"
	fetch libzip  libzip 1.1.3  nih.at/libzip               "--host=${HOST_NATIVE} --disable-shared --enable-static CFLAGS=-DZIP_STATIC"
	fetch libyaml yaml   0.1.7  pyyaml.org/download/libyaml "--host=${HOST_NATIVE} --disable-shared --enable-static CFLAGS=-DYAML_DECLARE_STATIC"
}

prepare
echo "Build vita-toolchain..."
cd ${CWD}
mkdir -p build
cd build
cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .. # work without ${CMAKE_FLAGS}
make all test
