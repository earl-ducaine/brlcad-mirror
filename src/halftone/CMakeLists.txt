set(HALFTONE_INCLUDE_DIRS
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES HALFTONE_INCLUDE_DIRS)
include_directories(${HALFTONE_INCLUDE_DIRS})

set(halftone_SOURCES
  main.c
  sharpen.c
  tone_classic.c
  tone_floyd.c
  tone_folly.c
  tone_simple.c
  tonescale.c
  )

BRLCAD_ADDEXEC(halftone "${halftone_SOURCES}" "librt;libfb")

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
