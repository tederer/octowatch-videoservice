#!/bin/bash

scriptDir=$(cd $(dirname $0) && pwd)
cd $scriptDir

BUILD_DIR=./build

if [ ! -d $BUILD_DIR ]; then
   echo "build folder ($BUILD_DIR) does not exist -> setting up meson ..."
   meson setup $BUILD_DIR
   exitCode=$?
   if [ $exitCode -ne 0 ]; then
      echo "ERROR: meson returned exit code $exitCode"
      exit $exitCode
   fi
fi

cd $BUILD_DIR
meson compile -j 1
