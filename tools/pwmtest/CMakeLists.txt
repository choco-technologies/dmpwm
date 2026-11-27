# pwmtest - manual dmpwm device test tool (added via add_subdirectory() from the top-level CMakeLists.txt)
set(DMOD_STACK_SIZE 4096)
set(DMOD_DMR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/pwmtest.dmr)

dmod_add_executable(pwmtest ${DMOD_MODULE_VERSION}
    main.c
)

# dmosi: sleep in the sweep. dmdrvi: the ioctl numbering.
dmod_link_modules(pwmtest
    dmosi
    dmdrvi@>=2.5
)

target_link_libraries(pwmtest dmpwm_if)
