set(COMPONENT_SRCDIRS ". cpu sound")
set(COMPONENT_ADD_INCLUDEDIRS ". cpu sound")
set(COMPONENT_REQUIRES "retro-go")
register_component()
rg_setup_compile_options(-O3)
