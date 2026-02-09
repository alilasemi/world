#!/bin/bash

# GLFW
if false; then
    git clone https://github.com/glfw/glfw external/glfw
    cd external/glfw
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=.
    make install -j
    cd ../../..
fi

# GLAD
# Go here:
# http://glad.dav1d.de/
# Select GL version 3.3, Profile = Core

# GoogleTest
if false; then
    git clone https://github.com/google/googletest external/googletest
    cd external/googletest
    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=. ..
    make install -j
    cd ../../..
fi
