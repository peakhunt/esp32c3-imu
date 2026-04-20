set(srcs "nvs_helper.c")

set(includes ".")

idf_component_register(
    SRCS ${srcs}
    REQUIRES nvs_flash
    INCLUDE_DIRS ${includes}
)
