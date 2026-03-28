set(UNITY_DIR ${CMAKE_BINARY_DIR}/_deps/unity-src/src/)

add_executable(test_stream_reader 	test_stream_reader.c
			${CMAKE_SOURCE_DIR}/stream_reader.c	${CMAKE_SOURCE_DIR}/stream_reader.h
								unity.h)
target_link_libraries(test_stream_reader PRIVATE unity)
target_include_directories(test_stream_reader PRIVATE ${UNITY_DIR} ${CMAKE_SOURCE_DIR})

if (BUILD_TESTING)
	add_test(NAME TEST_STREAM_READER COMMAND test_stream_reader)
endif ()

