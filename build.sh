#!/bin/bash

set -eu

CWD=$PWD
BUILDDIR=$PWD/builds
DEPSDIR=$PWD/builds/deps_build
JOBS=-j`getconf _NPROCESSORS_ONLN || sysctl kern.smp.cpus | sed 's/kern.smp.cpus: //'` || true

if [ ! -d buildscripts ]; then
    echo "[Step 0.0] Clone buildscripts..."
    git clone https://github.com/vitasdk/buildscripts
else
    echo "[Step 0.0] Update buildscripts..."
    cd buildscripts
    git fetch origin
    git reset --hard origin/master
fi

echo "[Step 1.0] Prepare buildscripts..."
mkdir -p ${BUILDDIR}
cd ${BUILDDIR}
cmake ../buildscripts

echo "[Step 1.1] Build zlib..."
cmake --build . --target zlib_build -- ${JOBS}

echo "[Step 1.2] Build libzip..."
cmake --build . --target libzip_build -- ${JOBS}

echo "[Step 1.3] Build libelf..."
cmake --build . --target libelf_build -- ${JOBS}

echo "[Step 1.4] Build libyaml..."
cmake --build . --target libyaml_build -- ${JOBS}

echo "[Step 2.0] Build vita-toolchain..."
cd ${CWD}
mkdir -p build
cd build
cmake -G"Unix Makefiles" -DCMAKE_C_FLAGS_RELEASE:STRING="-O3 -DNDEBUG -DZIP_STATIC" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=`[ "$OS" = Windows_NT ] && echo ON || echo OFF` -Dlibyaml_INCLUDE_DIRS=${DEPSDIR}/include/ -Dlibyaml_LIBRARY=${DEPSDIR}/lib/libyaml.a -Dlibelf_INCLUDE_DIR=${DEPSDIR}/include -Dlibelf_LIBRARY=${DEPSDIR}/lib/libelf.a -Dzlib_INCLUDE_DIR=${DEPSDIR}/include/ -Dzlib_LIBRARY=${DEPSDIR}/lib/libz.a -Dlibzip_INCLUDE_DIR=${DEPSDIR}/include/ -Dlibzip_CONFIG_INCLUDE_DIR=${DEPSDIR}/lib/libzip/include -Dlibzip_LIBRARY=${DEPSDIR}/lib/libzip.a  ../
cmake --build . --clean-first -- ${JOBS}
