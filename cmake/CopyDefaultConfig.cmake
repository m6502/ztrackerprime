# Seed a fresh runtime directory without overwriting saved user settings.
if(NOT DEFINED SOURCE OR NOT DEFINED DESTINATION)
  message(FATAL_ERROR "SOURCE and DESTINATION are required")
endif()

if(NOT EXISTS "${DESTINATION}")
  configure_file("${SOURCE}" "${DESTINATION}" COPYONLY)
endif()
