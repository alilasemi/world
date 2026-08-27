SHELL := /bin/bash
.DEFAULT_GOAL := all

# Default to parallel builds so independent .cu files compile concurrently.
# --output-sync=target keeps each recipe's output contiguous.
# Respects an explicit `make -jN` from the command line.
ifeq ($(filter -j%,$(MAKEFLAGS)),)
MAKEFLAGS += -j$(shell nproc) --output-sync=target
endif

# -- Environment checks -- #
ifneq ($(shell uname -s),Linux)
$(error This Makefile currently only supports Linux (uses X11/GLX linker flags). Detected: $(shell uname -s))
endif

# -- Compiler / CUDA auto-detection -- #
CXX ?= g++

# Resolve the CUDA toolkit location: respect CUDA_HOME/CUDA_PATH if set,
# else derive it from `nvcc` on PATH, else fall back to the common symlink.
CUDA_HOME ?= $(or $(CUDA_PATH),$(shell command -v nvcc >/dev/null 2>&1 && dirname $$(dirname $$(command -v nvcc)) || echo /usr/local/cuda))
NVCC := $(CUDA_HOME)/bin/nvcc
ifeq (,$(wildcard $(NVCC)))
$(error CUDA toolkit not found at '$(CUDA_HOME)/bin/nvcc'. Install CUDA, or set CUDA_HOME=/path/to/cuda)
endif

# GPU architecture: auto-detect the local GPU at compile time. Override with
# `make GPU_ARCH=sm_75` for cross-compiling or unusual setups.
GPU_ARCH ?= native
cuda_flags = -arch=$(GPU_ARCH)

$(info Using CXX=$(CXX) CUDA_HOME=$(CUDA_HOME) GPU_ARCH=$(GPU_ARCH))

# Executable names
exe = world
test_exe = test
exe_profile = profile
exe_stability = stability_and_accuracy
exe_model_problem = model_problem
# Directories
src_dir = src
build_dir = build
bin_dir = $(build_dir)/bin
test_dir = test
obj_dir = $(build_dir)/obj
test_obj_dir = $(build_dir)/obj

# Files
test_src = $(wildcard $(test_dir)/*.cpp)
list_of_cpp_files = color_map particle_drawer sim_config
list_of_cuda_files = kernel compute_rhs_kernel particle_dynamics find_neighbors_kernel semi_implicit_euler_kernel backward_euler_picard_kernel energy_kernel
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
stability_obj = $(obj_dir)/stability_and_accuracy.o
model_problem_obj = $(obj_dir)/model_problem.o

# -- External dependencies (auto-built if missing) -- #
glfw_lib = external/glfw/build/lib/libglfw3.a
gtest_lib = external/googletest/build/lib/libgtest.a
usockets_lib = external/uWebSockets/uSockets/uSockets.a
glad_src = external/glad/src/glad.c

ifeq (,$(wildcard $(glad_src)))
$(error GLAD not found in external/glad/. Generate it at https://glad.dav1d.de/ (GL version 3.3, Profile = Core), download the zip, and extract it into external/glad/)
endif

$(glfw_lib):
	if [ ! -d external/glfw ]; then git clone https://github.com/glfw/glfw external/glfw; fi
	cmake -S external/glfw -B external/glfw/build -DCMAKE_INSTALL_PREFIX=external/glfw/build
	cmake --build external/glfw/build --target install -j

$(gtest_lib):
	if [ ! -d external/googletest ]; then git clone https://github.com/google/googletest external/googletest; fi
	cmake -S external/googletest -B external/googletest/build -DCMAKE_INSTALL_PREFIX=external/googletest/build
	cmake --build external/googletest/build --target install -j

$(usockets_lib):
	if [ ! -d external/uWebSockets ]; then git clone --recurse-submodules https://github.com/uNetworking/uWebSockets external/uWebSockets; fi
	$(MAKE) -C external/uWebSockets/uSockets

.PHONY: deps
deps: $(glfw_lib) $(gtest_lib) $(usockets_lib)

# Paths to includes
include_paths = $(src_dir) $(CUDA_HOME)/include external/glfw/build/include/ external/glad/include/ external/googletest/build/include external/uWebSockets/src/ external/uWebSockets/uSockets/src/
# Warnings
warnings = -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wreorder
# Compiler flags
flags = $(foreach dir, $(include_paths), -I$(dir)) -std=c++17 -O3
debug: flags += -g -DDEBUG
debug: all
# Automatic header dependency tracking. -MMD writes a .d file alongside each .o
# listing the (non-system) headers that object depends on; -MP additionally emits
# a phony target for each of those headers, so deleting or renaming a header
# yields a rebuild rather than a "No rule to make target" hard error. Both g++
# and nvcc accept these flags directly.
#
# Without this, the pattern rules below depend only on the matching source file,
# so editing a widely-included header (e.g. particle_dynamics.h) and running an
# incremental `make` silently links a stale .o compiled against the old struct
# layout -- which showed up at runtime as "stack smashing detected" with garbage
# timing values, and used to require `make clean` after every header edit.
dep_flags = -MMD -MP
# Libraries and locations
ldlibs = -Lexternal/googletest/build/lib -lgtest -lgtest_main -Lexternal/glfw/build/lib -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl external/uWebSockets/uSockets/uSockets.a -lz -lssl -lcrypto -lpthread
cuda_ldlibs = -L$(CUDA_HOME)/lib64 -lcudart

.PHONY: all
all: deps directories $(bin_dir)/$(exe)

.PHONY: profile
profile: deps directories $(bin_dir)/$(exe_profile)

.PHONY: stability
stability: deps directories $(bin_dir)/$(exe_stability)

.PHONY: model_problem
model_problem: deps directories $(bin_dir)/$(exe_model_problem)

.PHONY: test
test: deps directories $(bin_dir)/$(test_exe)
	./$(bin_dir)/$(test_exe)

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
	$(CXX) $(flags) $(dep_flags) -o $@ -c $< $(ldlibs)

# Internal source files
$(obj_dir)/%.o: $(src_dir)/%.cpp
	$(CXX) $(flags) $(warnings) $(dep_flags) -o $@ -c $<

# Internal source test files
$(test_obj_dir)/%.o: $(test_dir)/%.cpp
	$(CXX) $(flags) $(warnings) $(dep_flags) -o $@ -c $<

# CUDA files
$(obj_dir)/%.o: $(src_dir)/%.cu
	$(NVCC) $(flags) $(cuda_flags) $(dep_flags) -o $@ -c $<

# -- Executables --#
$(bin_dir)/$(exe): $(main_obj) $(obj)
	$(NVCC) $(flags) $(cuda_flags) -o $@ $^ $(ldlibs) $(cuda_ldlibs)

$(bin_dir)/$(exe_profile): $(profile_obj) $(obj)
	$(NVCC) $(flags) $(cuda_flags) -o $@ $^ $(ldlibs) $(cuda_ldlibs)

$(bin_dir)/$(exe_stability): $(stability_obj) $(obj)
	$(NVCC) $(flags) $(cuda_flags) -o $@ $^ $(ldlibs) $(cuda_ldlibs)

$(bin_dir)/$(exe_model_problem): $(model_problem_obj) $(obj)
	$(NVCC) $(flags) $(cuda_flags) -o $@ $^ $(ldlibs) $(cuda_ldlibs)

$(bin_dir)/$(test_exe): $(test_obj) $(obj)
	$(CXX) $(flags) $(warnings) -o $@ $^ $(ldlibs) $(cuda_ldlibs)

# -- Header dependencies -- #
# Pull in the .d files generated by $(dep_flags) above. The wildcard makes this a
# no-op on a clean tree (nothing to include yet); `make clean` removes them with
# the rest of $(build_dir).
-include $(wildcard $(obj_dir)/*.d)
