set(UNITY_DIR ${CMAKE_BINARY_DIR}/_deps/unity-src/src/)

if (BUILD_TESTING)
	add_executable(test_reader test_reader.c ${CMAKE_SOURCE_DIR}/reader.c
		${CMAKE_SOURCE_DIR}/reader.h ${UNITY_DIR}/unity.h)
	target_link_libraries(test_reader PRIVATE unity)
	target_include_directories(test_reader PRIVATE ${UNITY_DIR} ${CMAKE_SOURCE_DIR})

	add_executable(test_http_message test_http_message.c ${CMAKE_SOURCE_DIR}/http_message.c
		${CMAKE_SOURCE_DIR}/http_message.h ${CMAKE_SOURCE_DIR}/reader.c
		${CMAKE_SOURCE_DIR}/reader.h ${UNITY_DIR}/unity.h ${CMAKE_SOURCE_DIR}/abnf.c ${CMAKE_SOURCE_DIR}/abnf.h ${CMAKE_SOURCE_DIR}/uri.c ${CMAKE_SOURCE_DIR}/uri.h)
	target_link_libraries(test_http_message PRIVATE unity)
	target_include_directories(test_http_message PRIVATE ${UNITY_DIR} ${CMAKE_SOURCE_DIR})

	add_test(NAME TEST_STREAM_READER COMMAND test_reader)
	add_test(NAME TEST_HTTP_MESSAGE COMMAND test_http_message)
endif ()

