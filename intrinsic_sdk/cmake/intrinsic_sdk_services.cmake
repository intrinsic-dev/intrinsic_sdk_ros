macro(intrinsic_sdk_generate_service_manifest)
  set(options)
  set(oneValueArgs SERVICE_NAME MANIFEST)
  set(multiValueArgs)

  cmake_parse_arguments(GENERATE_ARGS "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

  set(OUT_DIR ${CMAKE_BINARY_DIR}/${GENERATE_ARGS_SERVICE_NAME})
  file(MAKE_DIRECTORY ${OUT_DIR})

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
