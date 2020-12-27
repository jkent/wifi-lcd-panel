set(panel_SRCS
  dialog.c
  lcd.c
  panel.c
)

set(panel_INCLUDE_DIRS
  .
)

set(panel_REQUIRES
  driver
)

idf_component_register(
  SRCS ${panel_SRCS}
  INCLUDE_DIRS ${panel_INCLUDE_DIRS}
  REQUIRES ${panel_REQURIES}
)
