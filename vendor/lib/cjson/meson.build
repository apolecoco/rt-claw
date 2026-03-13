cjson_inc = include_directories('.')

cjson_lib = static_library('cjson',
    'cJSON.c',
    include_directories: cjson_inc,
)

cjson_dep = declare_dependency(
    link_with: cjson_lib,
    include_directories: cjson_inc,
)
