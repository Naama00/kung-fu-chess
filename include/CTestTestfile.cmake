# CMake generated Testfile for 
# Source directory: C:/Users/Admin/Documents/GitHub/kung-fu-chess
# Build directory: C:/Users/Admin/Documents/GitHub/kung-fu-chess/include
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(unit_tests "C:/Users/Admin/Documents/GitHub/kung-fu-chess/include/Debug/kungfu_tests.exe")
  set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;57;add_test;C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(unit_tests "C:/Users/Admin/Documents/GitHub/kung-fu-chess/include/Release/kungfu_tests.exe")
  set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;57;add_test;C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(unit_tests "C:/Users/Admin/Documents/GitHub/kung-fu-chess/include/MinSizeRel/kungfu_tests.exe")
  set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;57;add_test;C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(unit_tests "C:/Users/Admin/Documents/GitHub/kung-fu-chess/include/RelWithDebInfo/kungfu_tests.exe")
  set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;57;add_test;C:/Users/Admin/Documents/GitHub/kung-fu-chess/CMakeLists.txt;0;")
else()
  add_test(unit_tests NOT_AVAILABLE)
endif()
subdirs("_deps/catch2-build")
