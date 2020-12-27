set(main_SRCS
  clock.c
  main.c
  menu.c
)

set(main_INCLUDE_DIRS
  .
)

idf_component_register(
  SRCS ${main_SRCS}
  INCLUDE_DIRS ${main_INCLUDE_DIRS}
)
