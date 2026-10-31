
function(copyOutputs TARGET_FOLDER)
    cmake_parse_arguments(COPY "" "TARGET;COMMAND_TARGET;SUBDIRECTORY" "" ${ARGN})
    if(NOT COPY_TARGET)
        set(COPY_TARGET "${PROJECT_NAME}")
    endif()
    if(NOT COPY_COMMAND_TARGET)
        set(COPY_COMMAND_TARGET "${COPY_TARGET}")
    endif()
    if(NOT DEFINED COPY_SUBDIRECTORY)
        set(COPY_SUBDIRECTORY "SKSE/Plugins")
    endif()

    get_filename_component(OUTPUT_FOLDER "${TARGET_FOLDER}/${COPY_SUBDIRECTORY}" ABSOLUTE)

    message(STATUS "${COPY_TARGET} output folder: ${OUTPUT_FOLDER}")

    add_custom_command(
        TARGET "${COPY_COMMAND_TARGET}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${COPY_TARGET}>" "${OUTPUT_FOLDER}/$<TARGET_FILE_NAME:${COPY_TARGET}>"
        VERBATIM
    )

    # Copy each target's own linker symbols in Debug, including multi-config builds.
    if(MSVC)
        add_custom_command(
            TARGET "${COPY_COMMAND_TARGET}"
            POST_BUILD
            COMMAND "$<$<CONFIG:Debug>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_PDB_FILE:${COPY_TARGET}>;${OUTPUT_FOLDER}/$<TARGET_PDB_FILE_NAME:${COPY_TARGET}>>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()

endfunction()
