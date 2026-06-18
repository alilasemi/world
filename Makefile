SHELL := /bin/bash
# Compiler
CXX = g++
cuda_dir = /usr/local/cuda-13.1/
NVCC = $(cuda_dir)/bin/nvcc
# Executable names
exe = world
exe_cpu = world_cpu
test_exe = test
exe_profile = profile
# Directories
src_dir = src
build_dir = build
bin_dir = $(build_dir)/bin
test_dir = test
obj_dir = $(build_dir)/obj
test_obj_dir = $(build_dir)/obj

# Files
test_src = $(wildcard $(test_dir)/*.cpp)
list_of_cpp_files = color_map particle_drawer
list_of_cuda_files = kernel compute_rhs_kernel particle_dynamics_cuda
# Sources
CPP_SRCS := $(addprefix $(src_dir)/, $(list_of_cpp_files:=.cpp))
CU_SRCS  := $(addprefix $(src_dir)/, $(list_of_cuda_files:=.cu))

CPP_OBJS := $(CPP_SRCS:$(src_dir)/%.cpp=$(obj_dir)/%.o)
CU_OBJS  := $(CU_SRCS:$(src_dir)/%.cu=$(obj_dir)/%.o)

OBJS := $(CPP_OBJS) $(CU_OBJS)
# prepend with obj dir and postpend with .o
obj =
obj += $(addprefix $(obj_dir)/, $(list_of_cpp_files))
obj += $(addprefix $(obj_dir)/, $(list_of_cuda_files))
obj := $(addsuffix .o, $(obj))
# Add glad.o as well
obj += $(obj_dir)/glad.o

test_obj = $(test_src:$(test_dir)/%.cpp=$(test_obj_dir)/%.o)
profile_obj = $(obj_dir)/profiling_sim.o
main_obj = $(obj_dir)/broadcast.o
# Paths to includes
include_paths = $(src_dir) $(cuda_dir)/include external/glfw/build/include/ external/glad/include/ external/googletest/build/include external/uWebSockets/src/ external/uWebSockets/uSockets/src/
# Warnings
warnings = -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wreorder
# Compiler flags
flags = $(foreach dir, $(include_paths), -I$(dir)) -std=c++17 -O3
debug: flags += -g -DDEBUG
debug: all
# Libraries and locations
ldlibs = -Lexternal/googletest/build/lib -lgtest -lgtest_main -Lexternal/glfw/build/lib -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl external/uWebSockets/uSockets/uSockets.a -lz -lssl -lcrypto -lpthread
cuda_ldlibs = -L$(cuda_dir)/lib64 -lcudart
# Useful variables
empty =
test_suffix = _test

.PHONY: all
all: directories $(bin_dir)/$(exe) #$(bin_dir)/$(test_exe)

.PHONY: profile
profile: directories $(bin_dir)/$(exe_profile) #$(bin_dir)/$(test_exe)

# This is purely for testing purposes - allows you to print out anything
.PHONY: print
print:
	$(info $(main_obj))

.PHONY: directories
directories:
	if [ ! -d $(build_dir) ]; then mkdir $(build_dir); fi
	if [ ! -d $(bin_dir) ]; then mkdir $(bin_dir); fi
	if [ ! -d $(obj_dir) ]; then mkdir $(obj_dir); fi
	if [ ! -d $(test_obj_dir) ]; then mkdir $(test_obj_dir); fi

.PHONY: clean
clean:
	rm -rf $(build_dir)

# -- Objects -- #
# GLAD
$(obj_dir)/glad.o: external/glad/src/glad.c
	$(CXX) $(flags) -o $@ -c $< $(ldlibs)

# Internal source files
$(obj_dir)/%.o: $(src_dir)/%.cpp
	$(CXX) $(flags) $(warnings) -o $@ -c $<

# Internal source test files
$(test_obj_dir)/%.o: $(test_dir)/%.cpp
	$(CXX) $(flags) $(warnings) -o $@ -c $<

# CUDA files
$(obj_dir)/%.o: $(src_dir)/%.cu
	$(NVCC) $(flags) -o $@ -c $<

# -- Executables --#
$(bin_dir)/$(exe): $(main_obj) $(obj)
	$(NVCC) $(flags) -o $@ $^ $(ldlibs) $(cuda_ldlibs)

$(bin_dir)/$(exe_profile): $(profile_obj) $(obj) $(cuda_obj)
	$(NVCC) $(flags) -o $@ $^ $(ldlibs) $(cuda_ldlibs)

$(bin_dir)/$(test_exe): $(test_obj) $(filter-out $(obj_dir)/main_gfx.o, $(obj))
	$(CXX) $(flags) $(warnings) -o $@ $^ $(ldlibs)

