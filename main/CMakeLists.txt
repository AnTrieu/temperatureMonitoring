idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS ".")

 # requirements can't depend on config
set(COMPONENT_REQUIRES json)