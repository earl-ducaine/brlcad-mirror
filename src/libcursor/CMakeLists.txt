if(HAVE_TERMLIB)
  set(TERMLIB_INCLUDE_DIRS
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${TERMLIB_INCLUDE_DIR}
    )
  BRLCAD_LIB_INCLUDE_DIRS(termlib TERMLIB_INCLUDE_DIRS "")

  BRLCAD_ADDLIB(libcursor cursor.c "${TERMLIB_LIBRARY}")
  SET_TARGET_PROPERTIES(libcursor PROPERTIES VERSION 20.0.1 SOVERSION 20)
endif(HAVE_TERMLIB)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
