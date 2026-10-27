#!/bin/sh
# Fetch LLVM 19.1.7, add the FCPU target and build clang + llc.
#   llvm/setup.sh          clone, patch, link sources, configure, build
# The backend sources live in llvm/target/FCPU and llvm/clang; they are
# symlinked into the checkout. llvm/patches holds the small edits to
# existing LLVM files (Triple, clang target registration, and a libc++
# compatibility fix for the macOS 27 SDK).
set -e
cd "$(dirname "$0")"
if [ ! -d llvm-project ]; then
  git clone --depth 1 --branch llvmorg-19.1.7 https://github.com/llvm/llvm-project.git llvm-project
  git -C llvm-project apply ../patches/llvm-19.1.7-fcpu.patch
fi
ln -sfn ../../../../target/FCPU llvm-project/llvm/lib/Target/FCPU
ln -sfn ../../../../../clang/FCPU.h llvm-project/clang/lib/Basic/Targets/FCPU.h
ln -sfn ../../../../../clang/FCPU.cpp llvm-project/clang/lib/Basic/Targets/FCPU.cpp
# ccache, when installed, makes rebuilds after a backend change quick (used by CI)
CCACHE=""
command -v ccache >/dev/null && CCACHE="-DLLVM_CCACHE_BUILD=ON"
cmake -G Ninja -S llvm-project/llvm -B build -DCMAKE_BUILD_TYPE=Release $CCACHE \
  -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD="" -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=FCPU \
  -DBUILD_SHARED_LIBS=ON -DLLVM_OPTIMIZED_TABLEGEN=ON -DLLVM_INCLUDE_TESTS=OFF \
  -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
  -DCLANG_ENABLE_STATIC_ANALYZER=OFF -DCLANG_ENABLE_ARCMT=OFF \
  -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF -DLLVM_ENABLE_ZLIB=OFF
# NINJA_JOBS limits parallel jobs; some clang files need 2-3 GB each to compile
ninja -C build ${NINJA_JOBS:+-j $NINJA_JOBS} clang llc
