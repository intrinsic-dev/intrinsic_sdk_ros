macro(intrinsic_sdk_protobuf_generate)
  set(options)
  set(oneValueArgs NAME)
  set(multiValueArgs IMPORT_DIRS SOURCES)

  cmake_parse_arguments(GENERATE_ARGS "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

  set(GENERATE_ARGS_TARGET ${GENERATE_ARGS_NAME}_protos)

  set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${GENERATE_ARGS_NAME})

  file(MAKE_DIRECTORY ${OUT_DIR})

  if(GENERATE_ARGS_IMPORT_DIRS)
    set(IMPORT_DIRS ${GENERATE_ARGS_IMPORT_DIRS})
  else()
    set(IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  list(APPEND IMPORT_DIRS ${intrinsic_sdk_PROTO_DIR} ${googleapis_SOURCE_DIR})

  set(DESCRIPTOR_SET ${OUT_DIR}/${GENERATE_ARGS_NAME}_protos.desc)

  add_library(${GENERATE_ARGS_TARGET} STATIC ${GENERATE_ARGS_SOURCES})

  protobuf_generate(
    TARGET ${GENERATE_ARGS_TARGET}
    LANGUAGE cpp
    IMPORT_DIRS ${IMPORT_DIRS}
    PROTOC_OUT_DIR ${OUT_DIR}
  )

  target_include_directories(${GENERATE_ARGS_TARGET}
    PUBLIC
    ${OUT_DIR})
  target_link_libraries(${GENERATE_ARGS_TARGET} PUBLIC absl::cordz_functions protobuf::libprotobuf intrinsic_sdk::intrinsic_sdk)

  set(PROTOC_ARGS "")

  foreach(DIR ${IMPORT_DIRS})
    list(APPEND PROTOC_ARGS "-I${DIR}")
  endforeach()

  list(APPEND PROTOC_ARGS "--include_imports" "--include_source_info" "--descriptor_set_out=${DESCRIPTOR_SET}")
  list(APPEND PROTOC_ARGS "${GENERATE_ARGS_SOURCES}")

  add_custom_command(
    OUTPUT ${DESCRIPTOR_SET}
    COMMAND protobuf::protoc
    ARGS ${PROTOC_ARGS}
    DEPENDS ${protobuf_PROTOC_EXE}
    COMMENT "Generating skill descriptor set for: ${GENERATE_ARGS_NAME}"
    VERBATIM
  )

  add_custom_target(${GENERATE_ARGS_NAME}_desc DEPENDS ${DESCRIPTOR_SET})
endmacro()
