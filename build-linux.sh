#!/bin/bash -e
# build-linux.sh

CMAKE_FLAGS='-DLINUX_LOCAL_DEV=true -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-7 -DCMAKE_CXX_COMPILER=g++-7'

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/"

# Move into the build directory, run CMake, and compile the project
mkdir -p linux-build
pushd linux-build
cmake .. ${CMAKE_FLAGS}
make -j$(nproc)
popd

# Copy the Sys folder in
cp -r -n ${DATA_SYS_PATH} ${BINARY_PATH}

touch ./linux-build/Binaries/portable.txt
