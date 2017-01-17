#!/bin/bash
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
CWD=$PWD
DOWNLOADDIR=$PWD/download
INSTALLDIR=$PWD/install
SRCDIR=$PWD/sources
SRCRELDIR=sources
BUILDDIR=$PWD/builds
JOBS=-j2
ZLIB_VERSION=1.2.8
LIBZIP_VERSION=1.1.3
LIBELF_VERSION=0.8.13
LIBYAML_VERSION=0.1.7
echo "[Step 0.0] Clone buildscripts..."
git clone https://github.com/vitasdk/buildscripts
PATCHDIR=$PWD/buildscripts/patches
echo "[Step 1.0] Create dirs..."
mkdir -p ${DOWNLOADDIR}
mkdir -p ${INSTALLDIR}
mkdir -p ${SRCDIR}
mkdir -p ${BUILDDIR}
echo "[Step 1.1] Build zlib..."
cd ${DOWNLOADDIR}
curl -L -O http://zlib.net/zlib-${ZLIB_VERSION}.tar.xz
tar xJf zlib-${ZLIB_VERSION}.tar.xz -C ${SRCDIR}
cd ${SRCDIR}/zlib-${ZLIB_VERSION}
mkdir -p ${BUILDDIR}/zlib-${ZLIB_VERSION}
BINARY_PATH=${INSTALLDIR}/bin INCLUDE_PATH=${INSTALLDIR}/include LIBRARY_PATH=${INSTALLDIR}/lib 
${SRCDIR}/zlib-${ZLIB_VERSION}/configure --static --prefix=${INSTALLDIR} 
make ${JOBS}
make install
echo "[Step 1.2] Build libzip..."
cd ${DOWNLOADDIR}
curl -L -O https://nih.at/libzip/libzip-${LIBZIP_VERSION}.tar.xz
tar xJf libzip-${LIBZIP_VERSION}.tar.xz -C ${SRCDIR}
cd ${SRCDIR}/libzip-${LIBZIP_VERSION}
patch < ${PATCHDIR}/libzip.patch
mkdir -p ${BUILDDIR}/libzip-${LIBZIP_VERSION}
cd ${BUILDDIR}/libzip-${LIBZIP_VERSION}
CFLAGS='-DZIP_STATIC' ${SRCDIR}/libzip-${LIBZIP_VERSION}/configure --host=${HOST_NATIVE} --prefix=$INSTALLDIR --disable-shared --enable-static
make ${JOBS} -C lib install

echo "[Step 1.3] Build libelf..."
cd ${DOWNLOADDIR}
curl -L -O http://www.mr511.de/software/libelf-${LIBELF_VERSION}.tar.gz
tar xzf libelf-${LIBELF_VERSION}.tar.gz -C ${SRCDIR}
cd ${SRCDIR}/libelf-${LIBELF_VERSION}
patch < ${PATCHDIR}/libelf.patch
mkdir -p ${BUILDDIR}/libelf-${LIBELF_VERSION}
cd ${BUILDDIR}/libelf-${LIBELF_VERSION}
${SRCDIR}/libelf-${LIBELF_VERSION}/configure --host=${HOST_NATIVE} --prefix=$INSTALLDIR
make ${JOBS} install

echo "[Step 1.4] Build libyaml..."
cd ${DOWNLOADDIR}
curl -L -O http://pyyaml.org/download/libyaml/yaml-${LIBYAML_VERSION}.tar.gz
tar xzf yaml-${LIBYAML_VERSION}.tar.gz -C ${SRCDIR}
mkdir -p ${BUILDDIR}/yaml-${LIBYAML_VERSION}
cd ${BUILDDIR}/yaml-${LIBYAML_VERSION}
${SRCDIR}/yaml-${LIBYAML_VERSION}/configure --host=${HOST_NATIVE} --prefix=$INSTALLDIR --disable-shared --enable-static "CFLAGS=-DYAML_DECLARE_STATIC"
make ${JOBS} install

echo "[Step 2.0] Build vita-toolchain..."
cd ${CWD}
mkdir build
cd build
cmake -G"Unix Makefiles" -DCMAKE_C_FLAGS_RELEASE:STRING="-O3 -DNDEBUG -DZIP_STATIC" -DCMAKE_BUILD_TYPE=Release -Dlibyaml_INCLUDE_DIRS=$INSTALLDIR/include/ -Dlibyaml_LIBRARY=$INSTALLDIR/lib/libyaml.a -Dlibelf_INCLUDE_DIR=$INSTALLDIR/include -Dlibelf_LIBRARY=$INSTALLDIR/lib/libelf.a -Dzlib_INCLUDE_DIR=$INSTALLDIR/include/ -Dzlib_LIBRARY=$INSTALLDIR/lib/libz.a -Dlibzip_INCLUDE_DIR=$INSTALLDIR/include/ -Dlibzip_CONFIG_INCLUDE_DIR=$INSTALLDIR/lib/libzip/include -Dlibzip_LIBRARY=$INSTALLDIR/lib/libzip.a  ../
make 
