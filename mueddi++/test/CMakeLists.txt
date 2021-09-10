add_executable(test test.cc)

add_executable(dawg_test dawg_test.cc)

target_link_libraries(test LINK_PUBLIC mueddi)

target_link_libraries(dawg_test LINK_PUBLIC mueddi)
