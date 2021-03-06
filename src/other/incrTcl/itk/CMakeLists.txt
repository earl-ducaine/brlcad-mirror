#                     C M A K E L I S T S . T X T
# ITK
#
# Copyright (c) 2010 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# *******************************************************************
# ***                    Tcl CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 3.1.3)

# set CMake project name
PROJECT(ITK)

# Without Tcl, Tk and Itcl sources, we can't build Itk.  Use TCL_SRC_PREFIX
# and TCL_BIN_PREFIX to identify the location of the Tcl we are to use.
IF(TCL_SRC_PREFIX AND TCL_BIN_PREFIX AND ITCL_SRC_PREFIX AND ITCL_BIN_PREFIX AND TK_SRC_PREFIX AND TK_BIN_PREFIX)
  SET(TCL_INCLUDE_DIRS ${TCL_SRC_PREFIX}/generic ${TCL_SRC_PREFIX}/libtommath)
  SET(ITCL_INCLUDE_DIRS ${ITCL_SRC_PREFIX}/generic)
  SET(TK_INCLUDE_PATH ${TK_SRC_PREFIX}/generic ${TK_SRC_PREFIX}/bitmaps)
  IF(WIN32)
    SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SRC_PREFIX}/win)
    SET(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${TK_SRC_PREFIX}/xlib ${TK_SRC_PREFIX}/win)
  ELSE(WIN32)
    SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SRC_PREFIX}/unix)
  ENDIF(WIN32)
  # Locate the TCL libraries.  We allow these to be overridden so that a parent
  # build can use a CMake target instead of an actual library path.
  IF(NOT TCL_LIBRARY)
    find_library(TCL_LIBRARY tcl PATHS ${TCL_BIN_PREFIX}/lib)
  ENDIF(NOT TCL_LIBRARY)
  IF(NOT TCL_STUB_LIBRARY)
    find_library(TCL_STUB_LIBRARY tclstub PATHS ${TCL_BIN_PREFIX}/lib)
  ENDIF(NOT TCL_STUB_LIBRARY)
  IF(NOT ITCL_LIBRARY)
    find_library(ITCL_LIBRARY itcl PATHS ${ITCL_BIN_PREFIX}/lib)
  ENDIF(NOT ITCL_LIBRARY)
  IF(NOT ITCL_STUB_LIBRARY)
    find_library(ITCL_STUB_LIBRARY itclstub PATHS ${ITCL_BIN_PREFIX}/lib)
  ENDIF(NOT ITCL_STUB_LIBRARY)
  IF(NOT TK_LIBRARY)
    find_library(TK_LIBRARY tcl PATHS ${TK_BIN_PREFIX}/lib)
  ENDIF(NOT TK_LIBRARY)
  IF(NOT TK_STUB_LIBRARY)
    find_library(TK_STUB_LIBRARY tclstub PATHS ${TK_BIN_PREFIX}/lib)
  ENDIF(NOT TK_STUB_LIBRARY)
  IF(NOT TCL_LIBRARY OR NOT TCL_STUB_LIBRARY OR NOT TK_LIBRARY OR NOT TK_STUB_LIBRARY OR NOT ITCL_LIBRARY OR NOT ITCL_STUB_LIBRARY)
    MESSAGE(FATAL_ERROR "Could not find at least one of Tcl, Itcl or Tk libraries in ${TCL_BIN_PREFIX}, ${ITCL_BIN_PREFIX}, and ${TK_BIN_PREFIX} - please specify the parent directory for the libraries in the TCL_BIN_PREFIX variable (e.g. \${TCL_BIN_PREFIX}/lib)")
  ENDIF(NOT TCL_LIBRARY OR NOT TCL_STUB_LIBRARY OR NOT TK_LIBRARY OR NOT TK_STUB_LIBRARY OR NOT ITCL_LIBRARY OR NOT ITCL_STUB_LIBRARY)
ELSE(TCL_SRC_PREFIX AND TCL_BIN_PREFIX AND ITCL_SRC_PREFIX AND ITCL_BIN_PREFIX AND TK_SRC_PREFIX AND TK_BIN_PREFIX)
  MESSAGE(FATAL_ERROR "Please specify the source and library locations of Tcl, Tk and Itcl using the TCL_SRC_PREFIX, TCL_BIN_PREFIX, TK_SRC_PREFIX, TK_BIN_PREFIX, ITCL_SRC_PREFIX and ITCL_BIN_PREFIX variables.")
ENDIF(TCL_SRC_PREFIX AND TCL_BIN_PREFIX AND ITCL_SRC_PREFIX AND ITCL_BIN_PREFIX AND TK_SRC_PREFIX AND TK_BIN_PREFIX)

# build shared libs by default
SET(BUILD_SHARED_LIBS ON)

# build static libs by default
SET(BUILD_STATIC_LIBS ON)

# version numbers
SET(ITK_VERSION_MAJOR 3)
SET(ITK_VERSION_MINOR 4)
SET(ITK_PATCH_LEVEL "0")

SET(ITK_VERSION "${ITK_VERSION_MAJOR}.${ITK_VERSION_MINOR}")

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
add_definitions(-DPACKAGE_NAME="itk")
add_definitions(-DPACKAGE_TARNAME="itk")
add_definitions(-DPACKAGE_VERSION="${ITK_PATCH_LEVEL}")
add_definitions(-DPACKAGE_BUGREPORT="")
add_definitions(-DSTDC_HEADERS=1)

# We're building Itk
add_definitions(-DBUILD_itk)

#--------------------------------------------------------------------
# We need to enable the threading macros found in tcl.h and tclInt.h.
# The use of the threading features is determined by the core the
# extension is loaded into, but we need to compile with these macros
# turned on.
#--------------------------------------------------------------------
add_definitions(-DTCL_THREADS=1)

if(MSVC AND NOT CMAKE_CL_64)
  add_definitions(-D_USE_32BIT_TIME_T)
endif(MSVC AND NOT CMAKE_CL_64)

MACRO(TCL_CHECK_INCLUDE_FILE filename var)
  CHECK_INCLUDE_FILE(${filename} ${var})
  IF(${var})
    add_definitions(-D${var}=1)
  ENDIF(${var})
ENDMACRO(TCL_CHECK_INCLUDE_FILE)

TCL_CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
TCL_CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
TCL_CHECK_INCLUDE_FILE(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
  add_definitions(-DNO_FSTATFS=1)
ENDIF(NOT HAVE_SYS_FSTATFS_H)
TCL_CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
TCL_CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
TCL_CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
TCL_CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
TCL_CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

IF(TK_GRAPHICS_SYSTEM STREQUAL "x11")
  find_package(X11)
ENDIF(TK_GRAPHICS_SYSTEM STREQUAL "x11")

SET(ITK_GENERIC_SRCS
  generic/itkStubInit.c
  generic/itkStubLib.c
  generic/itk_archetype.c
  generic/itk_cmds.c
  generic/itk_option.c
  generic/itk_util.c
  )

SET(ITK_WIN_SRCS
  win/dllEntryPoint.c
  )

SET(ITK_STUB_SRCS
  generic/itkStubLib.c
  )


SET(ITK_INCLUDE_DIRS ${ITK_SOURCE_DIR}/generic ${ITK_BINARY_DIR}/include)
MARK_AS_ADVANCED(ITK_INCLUDE_DIRS)

include_directories(
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_PATH}
  ${ITCL_INCLUDE_DIRS}
  ${ITK_INCLUDE_DIRS}
  ${TK_X11_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/include
  ${CMAKE_SOURCE_DIR}/include
  )

IF(WIN32)
  add_library(itk ${ITK_GENERIC_SRCS} ${ITK_WIN_SRCS})
  target_link_libraries(itk ${M_LIBRARY} ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY} ${ITCL_STUB_LIBRARY})
  SET_PROPERTY(TARGET itk APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
  SET_PROPERTY(TARGET itk APPEND PROPERTY COMPILE_DEFINITIONS USE_ITCL_STUBS)
  SET_PROPERTY(TARGET itk APPEND PROPERTY COMPILE_DEFINITIONS USE_TK_STUBS)
ELSE(WIN32)
  add_library(itk ${ITK_GENERIC_SRCS})
  target_link_libraries(itk ${TCL_LIBRARIES} ${ITCL_LIBRARY})
ENDIF(WIN32)
SET_TARGET_PROPERTIES(itk PROPERTIES VERSION ${ITK_VERSION} SOVERSION ${ITK_VERSION_MAJOR})
install(TARGETS itk
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

add_library(itkstub STATIC ${ITK_STUB_SRCS})
target_link_libraries(itkstub ${ITCL_STUB_LIBRARY} ${TCL_STUB_LIBRARIES})
SET_PROPERTY(TARGET itkstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
SET_PROPERTY(TARGET itkstub APPEND PROPERTY COMPILE_DEFINITIONS USE_ITCL_STUBS)
SET_PROPERTY(TARGET itkstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TK_STUBS)
install(TARGETS itkstub
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

TCL_PKGINDEX(itk Itk "${ITK_VERSION_MAJOR}.${ITK_VERSION_MINOR}")

SET(ITK_LIBRARY_FILES
  library/Archetype.itk
  library/Toplevel.itk
  library/Widget.itk
  library/itk.tcl
  library/tclIndex
  )
install(FILES ${ITK_LIBRARY_FILES} DESTINATION lib/itk${ITK_VERSION_MAJOR}.${ITK_VERSION_MINOR})

FOREACH(file ${ITK_LIBRARY_FILES})
  get_filename_component(filename ${file} NAME)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${file} ${CMAKE_BINARY_DIR}/lib/itk${ITK_VERSION_MAJOR}.${ITK_VERSION_MINOR}/${filename} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${file} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/lib/itk${ITK_VERSION_MAJOR}.${ITK_VERSION_MINOR}/${filename} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
ENDFOREACH(file ${ITK_LIBRARY_FILES})

SET(ITK_HDRS
  generic/itk.h
  generic/itkDecls.h
  )
install(FILES ${ITK_HDRS} DESTINATION include)
