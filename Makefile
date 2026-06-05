SHELL := /bin/bash
# Compiler
CXX = g++
cuda_dir = /usr/local/cuda-13.0/
NVCC = $(cuda_dir)/bin/nvcc
# Executable names
exe = real_time_fluid
exe_cpu = real_time_fluid_cpu
test_exe = real_time_fluid_test
# Directories
src_dir = src
build_dir = build
bin_dir = $(build_dir)/bin
test_dir = test
obj_dir = $(build_dir)/obj
test_obj_dir = $(build_dir)/obj
# Files
test_src = $(wildcard $(test_dir)/*.cpp)
src = $(wildcard $(src_dir)/*.cpp)
obj = $(src:$(src_dir)/%.cpp=$(obj_dir)/%.o) $(obj_dir)/glad.o
cuda_obj = $(obj_dir)/particle_dynamics_cuda.o
test_obj = $(test_src:$(test_dir)/%.cpp=$(test_obj_dir)/%.o)
# Paths to includes
include_paths = $(src_dir) external/glfw/build/include/ external/glad/include/ external/googletest/build/include /home/ali/projects/real-time-particles/uWebSockets/src/ /home/ali/projects/real-time-particles/uWebSockets/uSockets/src/
# Warnings
warnings = -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wreorder
# Compiler flags
flags = $(foreach dir, $(include_paths), -I$(dir)) -std=c++17 -O3
debug: flags += -g -DDEBUG
debug: all
# Libraries and locations
ldlibs = -Lexternal/googletest/build/lib -lgtest -lgtest_main -Lexternal/glfw/build/lib -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl /home/ali/projects/real-time-particles/uWebSockets/uSockets/uSockets.a -lz -lssl -lcrypto -lpthread
cuda_ldlibs = -L$(cuda_dir)/lib64 -lcudart
# Useful variables
empty =
test_suffix = _test

.PHONY: all
all: directories $(bin_dir)/$(exe) #$(bin_dir)/$(test_exe)

.PHONY: cpu
cpu: directories $(bin_dir)/$(exe_cpu) #$(bin_dir)/$(test_exe)

# This is purely for testing purposes - allows you to print out anything
.PHONY: print
print:
	$(info $(test_obj))

.PHONY: directories
directories:
	if [ ! -d $(build_dir) ]; then mkdir $(build_dir); fi
	if [ ! -d $(bin_dir) ]; then mkdir $(bin_dir); fi
	if [ ! -d $(obj_dir) ]; then mkdir $(obj_dir); fi
	if [ ! -d $(test_obj_dir) ]; then mkdir $(test_obj_dir); fi

.PHONY: clean
clean:
	rm -rf $(build_dir)

# -- Executables --#
$(bin_dir)/$(exe): $(obj) $(cuda_obj)
	$(CXX) $(flags) $(warnings) -o $@ $^ $(ldlibs)

$(bin_dir)/$(exe_cpu): $(obj)
	$(CXX) $(flags) $(warnings) -o $@ $^ $(ldlibs)

$(bin_dir)/$(test_exe): $(test_obj) $(filter-out $(obj_dir)/main_gfx.o, $(obj))
	$(CXX) $(flags) $(warnings) -o $@ $^ $(ldlibs)

# -- Objects -- #
# GLAD
$(obj_dir)/glad.o: external/glad/src/glad.c
	$(CXX) $(flags) -o $@ -c $< $(ldlibs)

# Internal source files
$(obj_dir)/%.o: $(src_dir)/%.cpp
	$(CXX) $(flags) $(warnings) -o $@ -c $< $(ldlibs)

# Internal source test files
$(test_obj_dir)/%.o: $(test_dir)/%.cpp
	$(CXX) $(flags) $(warnings) -o $@ -c $< $(ldlibs)

# CUDA files
$(obj_dir)/particle_dynamics_cuda.o: $(src_dir)/particle_dynamics_cuda.cu
	$(NVCC) $(flags) -o $@ -c $< $(ldlibs) $(cuda_ldlibs)
