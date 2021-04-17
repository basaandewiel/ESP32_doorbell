idf_component_register(SRCS "remote_log.c" "include/remote_log.h"
                    INCLUDE_DIRS "include"
                    )


cmake_minimum_required(VERSION 3.19)
set(COMPONENT_SRCS "remote_log.c" "include/remote_log.h")
set(COMPONENT_ADD_INCLUDEDIRS "include" ".")

#project(esp-remote-log)
#register_component()
