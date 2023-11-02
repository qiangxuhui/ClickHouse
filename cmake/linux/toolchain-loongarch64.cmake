# See linux/toolchain-x86_64.cmake for details about multiple load of toolchain file.
include_guard(GLOBAL)

set (CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set (CMAKE_SYSTEM_NAME "Linux")
set (CMAKE_SYSTEM_PROCESSOR "loongarch64")
set (CMAKE_C_COMPILER_TARGET "loongarch64-linux-gnu")
set (CMAKE_CXX_COMPILER_TARGET "loongarch64-linux-gnu")
set (CMAKE_ASM_COMPILER_TARGET "loongarch64-linux-gnu")

# Will be changed later, but somehow needed to be set here.
set (CMAKE_AR "ar")
set (CMAKE_RANLIB "ranlib")

set (TOOLCHAIN_PATH "/usr")

set (CMAKE_SYSROOT "/")

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --gcc-toolchain=${TOOLCHAIN_PATH}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --gcc-toolchain=${TOOLCHAIN_PATH}")
set (CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} --gcc-toolchain=${TOOLCHAIN_PATH}")

# Currently, lld does not work with the error:
# ld.lld: error: section size decrease is too large
# But GNU BinUtils work.

set (HAS_PRE_1970_EXITCODE "0" CACHE STRING "Result from TRY_RUN" FORCE)
set (HAS_PRE_1970_EXITCODE__TRYRUN_OUTPUT "" CACHE STRING "Output from TRY_RUN" FORCE)

set (HAS_POST_2038_EXITCODE "0" CACHE STRING "Result from TRY_RUN" FORCE)
set (HAS_POST_2038_EXITCODE__TRYRUN_OUTPUT "" CACHE STRING "Output from TRY_RUN" FORCE)
