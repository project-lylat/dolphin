#!/bin/bash -e
# build-linux.sh

CMAKE_FLAGS='-DLINUX_LOCAL_DEV=true -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14'

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/"

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake .. ${CMAKE_FLAGS}
make -j"$(nproc)"
popd

# Copy the Sys folder in, unless the build already put it there (upstream CMake links
# Data/Sys into Binaries, and cp refuses to copy a folder over that link)
if [ ! -e "${BINARY_PATH}Sys" ]; then
	cp -r ${DATA_SYS_PATH} ${BINARY_PATH}
fi

touch ./build/Binaries/portable.txt
