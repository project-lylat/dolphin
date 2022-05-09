#!/bin/bash -e
# build-mac.sh

QT_BREW_PATH=$(brew --prefix qt@5)
CMAKE_FLAGS="-DQt5_DIR=${QT_BREW_PATH}/lib/cmake/Qt5 -DENABLE_NOGUI=false"

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/Dolphin.app/Contents/Resources/"

export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib:/usr/lib/

if [[ -z "${CERTIFICATE_MACOS_APPLICATION}" ]]
    then
        echo "Building without code signing"
        CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="OFF"'
else
        echo "Building with code signing"
        CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="ON"'
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
make -j7
popd

# Copy the Sys folder in
echo "Copying Sys files into the bundle"
cp -Rfn "${DATA_SYS_PATH}" "${BINARY_PATH}"