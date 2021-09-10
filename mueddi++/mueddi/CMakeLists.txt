add_library(mueddi dawg.cc decoder.cc encoder.cc leven.cc mueddi.cc)

target_include_directories (mueddi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
