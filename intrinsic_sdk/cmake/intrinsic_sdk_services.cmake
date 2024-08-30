macro(intrinsic_sdk_generate_service_manifest)
  set(options)
  set(multiValueArgs)
  set(oneValueArgs SERVICE_NAME MANIFEST PARAMETER_DESCRIPTOR DEFAULT_CONFIGURATION PROTOS_TARGET)

  cmake_parse_arguments(GENERATE_ARGS "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

  set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

  if (NOT GENERATE_ARGS_DEFAULT_CONFIGURATION STREQUAL "")
    if (GENERATE_ARGS_PARAMETER_DESCRIPTOR STREQUAL "")
      message(ERROR "PARAMETER_DESCRIPTOR must be passed with DEFAULT_CONFIGURATION")
    endif()

    intrinsic_sdk_protobuf_generate(
      NAME ${GENERATE_ARGS_SERVICE_NAME}
      SOURCES ${GENERATE_ARGS_PARAMETER_DESCRIPTOR}
      TARGET ${GENERATE_ARGS_PROTOS_TARGET})

    add_custom_command(
      OUTPUT ${OUT_DIR}/default_config.binarypb
      COMMAND ${intrinsic_sdk_DIR}/../../../lib/intrinsic_sdk/textproto_to_binproto.py
      ARGS
        --descriptor_database
          ${intrinsic_sdk_DESCRIPTOR_DATABASE}
          ${OUT_DIR}/${GENERATE_ARGS_SERVICE_NAME}_protos.desc
        --message_type=google.protobuf.Any
        --textproto_in=${CMAKE_CURRENT_SOURCE_DIR}/${GENERATE_ARGS_DEFAULT_CONFIGURATION}
        --binproto_out=${OUT_DIR}/default_config.binarypb
      DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATE_ARGS_DEFAULT_CONFIGURATION}
        ${OUT_DIR}/${GENERATE_ARGS_SERVICE_NAME}_protos.desc
      COMMENT "Generating default config for ${GENERATE_ARGS_SERVICE_NAME}"
    )

    add_custom_target(
      ${GENERATE_ARGS_SERVICE_NAME}_default_config DEPENDS
        ${OUT_DIR}/default_config.binarypb
    )

  endif()


  add_custom_command(
    OUTPUT ${OUT_DIR}/service_manifest.binarypb
    COMMAND ${intrinsic_sdk_DIR}/../../../lib/intrinsic_sdk/textproto_to_binproto.py
    ARGS
      --descriptor_database ${intrinsic_sdk_DESCRIPTOR_DATABASE}
      --message_type=intrinsic_proto.services.ServiceManifest
      --textproto_in=${CMAKE_CURRENT_SOURCE_DIR}/${GENERATE_ARGS_MANIFEST}
      --binproto_out=${OUT_DIR}/service_manifest.binarypb
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATE_ARGS_MANIFEST}
    COMMENT "Generating service manifest for ${GENERATE_ARGS_SERVICE_NAME}"
  )

  add_custom_target(
    ${GENERATE_ARGS_SERVICE_NAME}_manifest DEPENDS
      ${OUT_DIR}/service_manifest.binarypb
  )
endmacro()
