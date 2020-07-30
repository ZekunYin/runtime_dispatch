cmake_minimum_required(VERSION 3.2)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED YES)


set(HWY_SOURCES
	src/dispatch.cpp
	src/compiler_specific.h
)

set(HWY_FLAGS
  # Avoid changing binaries based on the current time and date.
  -Wno-builtin-macro-redefined
  -D__DATE__="redacted"
  -D__TIMESTAMP__="redacted"
  -D__TIME__="redacted"

  # Optimizations
  -fmerge-all-constants

  # Warnings
  -Wall
  -Wextra
  -Wformat-security
  -Wno-unused-function
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wvla
)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  list(APPEND HWY_FLAGS
    -Wc++2a-extensions
    -Wfloat-overflow-conversion
    -Wfloat-zero-conversion
    -Wfor-loop-analysis
    -Wgnu-redeclared-enum
    -Winfinite-recursion
    -Wself-assign
    -Wstring-conversion
    -Wtautological-overlap-compare
    -Wthread-safety-analysis
    -Wundefined-func-template

    -fno-cxx-exceptions
    -fno-slp-vectorize
    -fno-vectorize

    # Use color in messages
    -Xclang -fdiagnostics-show-option -fcolor-diagnostics
  )
endif()

if (WIN32)
  list(APPEND HWY_FLAGS
    -Wno-c++98-compat-pedantic
    -Wno-cast-align
    -Wno-double-promotion
    -Wno-float-equal
    -Wno-format-nonliteral
    -Wno-global-constructors
    -Wno-language-extension-token
    -Wno-missing-prototypes
    -Wno-shadow
    -Wno-shadow-field-in-constructor
    -Wno-sign-conversion
    -Wno-unused-member-function
    -Wno-unused-template
    -Wno-used-but-marked-unused
    -Wno-zero-as-null-pointer-constant
  )
else()
  list(APPEND HWY_FLAGS
    -fmath-errno
    -fno-exceptions
  )
endif()


add_executable(demo ${HWY_SOURCES})
target_compile_options(demo PRIVATE ${HWY_FLAGS})
