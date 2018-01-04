#!/bin/bash

set -e
set -x

BASE_DIR=/ci_build
ROCKSDB_SRC=$BASE_DIR/rocksdb
TERARK_CORE_HOME=$BASE_DIR/terark-core


CompilerList=(
    g++-4.8
#    g++-4.9
#    g++-5.3
    g++-5.4
#    g++-6.1
#    g++-6.2
#    g++-6.3
#    g++-7.1
#    g++-7.2
)

function build() {
    PKG="pkg"
    BMI2=0
    CPU=-march=corei7

    if [ `uname` == Darwin ]; then
        CPU_NUM=`sysctl -n machdep.cpu.thread_count`
    else
        CPU_NUM=`nproc`
    fi

    tmpfile=`mktemp`
    COMPILER=`g++ tools/configure/compiler.cpp -o ${tmpfile}.exe && ${tmpfile}.exe && rm -f ${tmpfile}*`
    FLAGS="-include `pwd`/tools/configure/glibc_memcpy_fix.h "
    UNAME_MachineSystem=`uname -m -s | sed 's:[ /]:-:g'`
    echo PWD=`pwd`

    make ROCKSDB_SRC=$ROCKSDB_SRC TERARK_CORE_HOME=$TERARK_CORE_HOME PKG_WITH_STATIC=1 WITH_BMI2=$BMI2 DFADB_TARGET=DfaDB PKG_WITH_DBG=1 PKG_WITH_DEP=1 -j$CPU_NUM $PKG CXXFLAGS="${FLAGS}" CFLAGS="${FLAGS}" CPU=$CPU
}

function MakeTerarkZipRocks() {
	if [[ `uname` != Darwin && $Compiler != g++-4.7 ]]; then
		build ${ProgArgs[@]}
	fi
}


OLD_PATH=$PATH
OLD_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
UNAME_MachineSystem=`uname -m -s | sed 's:[ /]:-:g'`

for Compiler in ${CompilerList[@]}
do
    if test -e /opt/$Compiler/bin; then
        export PATH=/opt/$Compiler/bin:$OLD_PATH
        export LD_LIBRARY_PATH=/opt/$Compiler/lib64:$OLD_LD_LIBRARY_PATH
        export TerarkLibDir=${TERARK_CORE_HOME}/terark-fsa_all-${UNAME_MachineSystem}-${Compiler}-bmi2-0/lib

        MakeTerarkZipRocks
    else
        echo Not found compiler: $Compiler
    fi
done
