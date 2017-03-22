#!/bin/bash
sys=`uname`
if [ "$sys" = "Darwin" ];then
    node-gyp configure -- -f xcode
fi