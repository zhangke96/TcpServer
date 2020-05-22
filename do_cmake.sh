#!/bin/bash

if test -e build;then
    rm -rf build
fi

mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=debug "$@" ..
