###########################################################
## TARGETS
###########################################################

add_executable(uintr-pro-client client.c)
add_executable(uintr-pro-server server.c)
add_executable(uintr-pro uintr-pro.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(uintr-pro-client ipc-bench-common)
target_link_libraries(uintr-pro-server ipc-bench-common)
target_link_libraries(uintr-pro ipc-bench-common)
