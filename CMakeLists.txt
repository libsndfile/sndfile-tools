cmake_minimum_required(VERSION 3.12)

project(sndfile-tools
  VERSION 1.04
  DESCRIPTION "Collection of programs for operating on sound files"
  HOMEPAGE_URL "http://libsndfile.github.io/libsamplerate/"
  LANGUAGES C
)

include(CheckIncludeFile)
include(CheckLibraryExists)
include(CMakeDependentOption)
include(CPack)
include(GNUInstallDirs)
include(FeatureSummary)
include(CTest)

find_library(HAVE_LIBM m)
if(HAVE_LIBM)
  check_library_exists(m floor "" HAVE_FLOOR_IN_LIBM)
  if(HAVE_FLOOR_IN_LIBM)
    list(APPEND CMAKE_REQUIRED_LIBRARIES m)
  endif()
endif()

check_include_file(sys/wait.h HAVE_SYS_WAIT_H)

find_package(PkgConfig)

pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile>=1.0.19)
pkg_check_modules(SAMPLERATE REQUIRED IMPORTED_TARGET samplerate>=0.1.5)
pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3>=0.15.0)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo>=1.4.0)
pkg_check_modules(JACK IMPORTED_TARGET jack>=0.100)
if(JACK_FOUND)
  find_package(Threads)
endif()

configure_file(config.h.cmake config.h)
add_compile_definitions(HAVE_CONFIG_H)
include_directories(${PROJECT_BINARY_DIR})

cmake_dependent_option(ENABLE_JACK "Enable libjack" ON "ENABLE_JACK" OFF)

add_executable(sndfile-generate-chirp
  src/generate-chirp.c
  src/common.c
  src/common.h
)
target_link_libraries(sndfile-generate-chirp PRIVATE PkgConfig::SNDFILE)

add_executable(sndfile-spectrogram
  src/spectrogram.c
  src/window.c
  src/common.c
  src/window.h
  src/common.h
  src/spectrum.c
  src/spectrum.h
)
target_link_libraries(sndfile-spectrogram
  PRIVATE
    PkgConfig::SNDFILE
    PkgConfig::FFTW3
    PkgConfig::CAIRO
)

add_executable(sndfile-mix-to-mono
  src/mix-to-mono.c
  src/common.h
  src/common.c
)
target_link_libraries(sndfile-mix-to-mono PRIVATE PkgConfig::SNDFILE)

add_executable(sndfile-waveform
  src/waveform.c
  src/common.c
  src/common.h
)
target_link_libraries(sndfile-waveform
  PRIVATE
    PkgConfig::SNDFILE
    PkgConfig::CAIRO
)

add_executable(sndfile-resample
  src/resample.c
  src/common.c
  src/common.h
)
target_link_libraries(sndfile-resample
  PRIVATE
    PkgConfig::SNDFILE
    PkgConfig::SAMPLERATE
)

if(ENABLE_JACK)
  add_executable(sndfile-jackplay src/jackplay.c)
  target_link_libraries(sndfile-jackplay
    PRIVATE
      PkgConfig::SNDFILE
      PkgConfig::JACK
  )
  if(Threads_FOUND)
    target_link_libraries(sndfile-jackplay PRIVATE Threads::Threads)
  endif()
endif()

set(SNDFILE_TOOLS_TARGETS
  sndfile-generate-chirp
  sndfile-spectrogram
  sndfile-mix-to-mono
  sndfile-waveform
  sndfile-resample
)
if(ENABLE_JACK)
  list(APPEND SNDFILE_TOOLS_TARGETS sndfile-jackplay)
endif()

install(TARGETS ${SNDFILE_TOOLS_TARGETS}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(SNDFILE_TOOLS_MANS
  man/sndfile-generate-chirp.1
  man/sndfile-mix-to-mono.1
  man/sndfile-resample.1
  man/sndfile-spectrogram.1
  man/sndfile-waveform.1
)
if(ENABLE_JACK)
  list(APPEND SNDFILE_TOOLS_MANS man/sndfile-jackplay.1)
endif()

install(FILES ${SNDFILE_TOOLS_MANS} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

add_feature_info(ENABLE_JACK ENABLE_JACK "build sndfile-jackplay (requires libjack library).")

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)

if(BUILD_TESTING)
  add_executable(kaiser_window_test tests/kaiser_window_test.c src/window.h src/window.c)
  target_include_directories(kaiser_window_test PRIVATE ${PROJECT_SOURCE_DIR})
  add_test(COMMAND kaiser_window_test NAME kaiser_window_test)

  if(HAVE_SYS_WAIT_H)
    add_executable(common_tests tests/common_tests.c src/common.c src/common.h)
    target_include_directories(common_tests PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(COMMAND common_tests NAME common_tests)
  endif()
endif()
