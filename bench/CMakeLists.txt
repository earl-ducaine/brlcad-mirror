add_subdirectory(ref)

include_directories(${BU_INCLUDE_DIRS})

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
  -DCONSOLE
  )

BRLCAD_ADDEXEC(pixcmp pixcmp.c libbu)
set(BARK_SOURCES
  benchmark.c
  compute.c
  run.c
  clean.c
)
BRLCAD_ADDEXEC(bark "${BARK_SOURCES}" libbu NO_STRICT NO_INSTALL)

configure_file(run.sh "${CMAKE_CURRENT_BINARY_DIR}/benchmark" COPYONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/benchmark" DESTINATION ${BIN_DIR})
if(CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    configure_file(run.sh ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/benchmark COPYONLY)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
else(CMAKE_CONFIGURATION_TYPES)
  configure_file(run.sh ${CMAKE_BINARY_DIR}/bin/benchmark COPYONLY)
endif(CMAKE_CONFIGURATION_TYPES)

file(WRITE  "${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake" "message(\"---\")\n")
file(APPEND "${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake" "message(\"Run 'make benchmark-clean' in the bench directory to delete the logic\")\n")
file(APPEND "${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake" "message(\"and image files generated during the benchmark analysis.\")\n")

CMAKEFILES(
  CMakeLists.txt
  lgt.sh
  run.sh
  try.sh
  viewdiff.sh
  )

if(SH_EXEC)
  # CTest enabled short run of the benchmark suite
  add_test(NAME benchmark COMMAND ${SH_EXEC} "${CMAKE_BINARY_DIR}/bin/benchmark" run TIMEFRAME=1)
  set_tests_properties(benchmark PROPERTIES LABELS "Benchmark")
  add_custom_target(benchmark-check
    COMMAND ${CMAKE_CTEST_COMMAND} -L Benchmark --output-on-failure
    DEPENDS rt pixcmp BENCHMARK_PIX_cp PIX_LOG_cp
    )
  add_dependencies(benchmark-check bldg391.g m35.g moss.g sphflake.g star.g world.g)
  if(TARGET check)
    add_dependencies(check benchmark-check)
  endif(TARGET check)

  # Full run of the benchmark suite
  add_custom_target(benchmark
    COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/benchmark run
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/CMakeTmp/benchmsg.cmake"
    DEPENDS rt pixcmp BENCHMARK_PIX_cp PIX_LOG_cp
    )
  add_dependencies(benchmark bldg391.g m35.g moss.g sphflake.g star.g world.g)

  # benchmark cleanup targets
  add_custom_target(benchmark-clean
    ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/benchmark clean
    )
  add_custom_target(benchmark-clobber
    ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/benchmark clobber
    )
endif(SH_EXEC)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
