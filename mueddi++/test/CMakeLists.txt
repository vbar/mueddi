add_executable(test test.cc)

add_executable(dawg_test dawg_test.cc)

add_executable(crosstest crosstest.cc ingest.cc levenshtein.cc)

add_executable(levenshtein_test levenshtein_test.cc levenshtein.cc)

target_link_libraries(test LINK_PUBLIC mueddi)

target_link_libraries(dawg_test LINK_PUBLIC mueddi)

target_link_libraries(crosstest LINK_PUBLIC mueddi)
