set(AI_COMPAT_SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/compat_0.7.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.0.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.1.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.2.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.3.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.4.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.5.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.6.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.7.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.8.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.9.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.10.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_1.11.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_12.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_13.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_14.nut
	${CMAKE_CURRENT_SOURCE_DIR}/compat_15.nut
	# Founder Mode transit operators (see THIRD_PARTY.md).
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleAI-14.tar
	${CMAKE_CURRENT_SOURCE_DIR}/library/Pathfinder.Road-4.tar
	${CMAKE_CURRENT_SOURCE_DIR}/library/Pathfinder.Rail-1.tar
	${CMAKE_CURRENT_SOURCE_DIR}/library/Graph.AyStar-4.tar
	${CMAKE_CURRENT_SOURCE_DIR}/library/Graph.AyStar-6.tar
	${CMAKE_CURRENT_SOURCE_DIR}/library/Queue.BinaryHeap-1.tar
	# Founder Mode rival startups.
	${CMAKE_CURRENT_SOURCE_DIR}/FounderRival/info.nut
	${CMAKE_CURRENT_SOURCE_DIR}/FounderRival/main.nut
)

foreach(AI_COMPAT_SOURCE_FILE IN LISTS AI_COMPAT_SOURCE_FILES)
    string(REPLACE "${CMAKE_SOURCE_DIR}/bin/" "" AI_COMPAT_SOURCE_FILE_NAME "${AI_COMPAT_SOURCE_FILE}")
    string(CONCAT AI_COMPAT_BINARY_FILE "${CMAKE_BINARY_DIR}/" "${AI_COMPAT_SOURCE_FILE_NAME}")

    add_custom_command(OUTPUT ${AI_COMPAT_BINARY_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/ai/library ${CMAKE_BINARY_DIR}/ai/FounderRival
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${AI_COMPAT_SOURCE_FILE}
                    ${AI_COMPAT_BINARY_FILE}
            MAIN_DEPENDENCY ${AI_COMPAT_SOURCE_FILE}
            COMMENT "Copying ${AI_COMPAT_SOURCE_FILE_NAME}"
    )

    list(APPEND AI_COMPAT_BINARY_FILES ${AI_COMPAT_BINARY_FILE})
endforeach()

# Create a new target which copies all compat files
add_custom_target(ai_compat_files
        DEPENDS ${AI_COMPAT_BINARY_FILES}
)

add_dependencies(openttd
    ai_compat_files
)
