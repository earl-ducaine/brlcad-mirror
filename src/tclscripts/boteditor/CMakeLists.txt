set(boteditor_TCLSCRIPTS
  botEditor.tcl
  botPropertyBox.tcl
  botTools.tcl
  )
BRLCAD_ADDDATA(boteditor_TCLSCRIPTS tclscripts/boteditor)
tclIndex_BUILD(tclscripts/boteditor)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
