macro(intrinsic_sdk_generate_skill_config)
  set(options)
  set(oneValueArgs SKILL_NAME MANIFEST)
  set(multiValueArgs)

  cmake_parse_arguments(GENERATE_ARGS "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

  set(OUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

  add_custom_command(
    OUTPUT ${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_manifest.pbbin
    COMMAND ${intrinsic_sdk_DIR}/../../../lib/intrinsic_sdk/textproto_to_binproto.py
    ARGS
      --descriptor_database
        ${intrinsic_sdk_DESCRIPTOR_DATABASE}
        ${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_protos.desc
      --message_type=intrinsic_proto.skills.Manifest
      --textproto_in=${CMAKE_CURRENT_SOURCE_DIR}/${GENERATE_ARGS_MANIFEST}
      --binproto_out=${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_manifest.pbbin
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${GENERATE_ARGS_MANIFEST}
    COMMENT "Generating skill manifest for ${GENERATE_ARGS_SKILL_NAME}"
  )

  add_custom_command(
    OUTPUT ${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_skill_config.pbbin
    COMMAND ${intrinsic_sdk_DIR}/../../../bin/skill_service_config_main
    ARGS
      --manifest_pbbin_filename=${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_manifest.pbbin
      --proto_descriptor_filename=${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_protos.desc
      --output_config_filename=${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_skill_config.pbbin
    COMMENT "Generating skill config for ${GENERATE_ARGS_SKILL_NAME}"
    DEPENDS
      ${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_manifest.pbbin
      ${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_protos.desc
  )

  add_custom_target(
    ${GENERATE_ARGS_SKILL_NAME}_skill_config DEPENDS
      ${OUT_DIR}/${GENERATE_ARGS_SKILL_NAME}_skill_config.pbbin
  )
endmacro()

macro(intrinsic_sdk_generate_skill)
  set(options)
  set(oneValueArgs SKILL_NAME MANIFEST PROTOS_TARGET)
  set(multiValueArgs SOURCES)

  cmake_parse_arguments(GENERATE_ARGS "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

  intrinsic_sdk_protobuf_generate(NAME ${GENERATE_ARGS_SKILL_NAME} SOURCES ${GENERATE_ARGS_SOURCES} TARGET ${GENERATE_ARGS_PROTOS_TARGET})
  intrinsic_sdk_generate_skill_config(SKILL_NAME ${GENERATE_ARGS_SKILL_NAME} MANIFEST ${GENERATE_ARGS_MANIFEST})
endmacro()

macro(intrinsic_sdk_skill_main)
  set(options)
  set(oneValueArgs SKILL_NAME CREATE_SKILL_FUNCTION CREATE_SKILL_HEADER MAIN_FILE)
  set(multiValueArgs)

  cmake_parse_arguments(GENERATE_SKILL_MAIN "${options}" "${oneValueArgs}"
                        "${multiValueArgs}" ${ARGN})

  configure_file(${intrinsic_sdk_DIR}/../templates/skill_main.cpp.in ${GENERATE_SKILL_MAIN_MAIN_FILE})
endmacro()
