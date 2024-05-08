#!/bin/bash

scriptDir=$(cd $(dirname $0) && pwd)
cd $scriptDir

./build/video_service

