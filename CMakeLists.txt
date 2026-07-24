cmake_minimum_required(VERSION 3.16)
project(GrokLinkOS VERSION 3.4.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(GLK_PLATFORM_HOST "Build host simulation" ON)
option(GLK_BUILD_TESTS "Build unit tests" ON)

add_compile_definitions(GLK_PLATFORM_HOST=1)

set(GLK_INC
  ${CMAKE_SOURCE_DIR}/kernel/include
  ${CMAKE_SOURCE_DIR}/hal/include
  ${CMAKE_SOURCE_DIR}/drivers/include
  ${CMAKE_SOURCE_DIR}/services/include
)

set(GLK_KERNEL_SRC
  kernel/src/glk_kernel.c
)

set(GLK_SVC_SRC
  services/src/safety/glk_policy.c
  services/src/audit/glk_audit.c
  services/src/agent/glk_agent.c
  services/src/agent/glk_catalog.c
  services/src/agent/glk_vault.c
  services/src/rpc/glk_rpc.c
  services/src/skill/glk_skill.c
  services/src/storage/glk_storage.c
  services/src/power/glk_power.c
  services/src/ml/glk_ml.c
)

set(GLK_DRV_SRC
  drivers/src/glk_radio.c
  drivers/src/glk_gpio.c
  drivers/src/glk_ir.c
  drivers/src/glk_nfc.c
  drivers/src/glk_cc1101.c
)

set(GLK_HAL_SRC
  hal/src/glk_hal_host.c
  hal/src/glk_hal_subghz.c
)

add_library(glk_core STATIC
  ${GLK_KERNEL_SRC}
  ${GLK_SVC_SRC}
  ${GLK_DRV_SRC}
  ${GLK_HAL_SRC}
)
target_include_directories(glk_core PUBLIC ${GLK_INC} ${CMAKE_SOURCE_DIR}/platform)

if(WIN32)
  target_link_libraries(glk_core PUBLIC ws2_32)
endif()

add_executable(groklink_os apps/boot/main.c)
target_link_libraries(groklink_os PRIVATE glk_core)
target_include_directories(groklink_os PRIVATE ${GLK_INC})
if(WIN32)
  target_link_libraries(groklink_os PRIVATE ws2_32)
endif()

if(GLK_BUILD_TESTS)
  enable_testing()
  add_executable(test_policy tests/unit/test_policy.c)
  target_link_libraries(test_policy PRIVATE glk_core)
  target_include_directories(test_policy PRIVATE ${GLK_INC})
  add_test(NAME policy COMMAND test_policy)

  add_executable(test_kernel tests/unit/test_kernel.c)
  target_link_libraries(test_kernel PRIVATE glk_core)
  target_include_directories(test_kernel PRIVATE ${GLK_INC})
  add_test(NAME kernel COMMAND test_kernel)

  add_executable(test_rpc_frame tests/unit/test_rpc_frame.c)
  target_link_libraries(test_rpc_frame PRIVATE glk_core)
  target_include_directories(test_rpc_frame PRIVATE ${GLK_INC})
  add_test(NAME rpc_frame COMMAND test_rpc_frame)

  add_executable(test_ml tests/unit/test_ml.c)
  target_link_libraries(test_ml PRIVATE glk_core)
  target_include_directories(test_ml PRIVATE ${GLK_INC})
  add_test(NAME ml COMMAND test_ml)

  add_executable(test_cc1101 tests/unit/test_cc1101.c)
  target_link_libraries(test_cc1101 PRIVATE glk_core)
  target_include_directories(test_cc1101 PRIVATE ${GLK_INC})
  add_test(NAME cc1101 COMMAND test_cc1101)

  add_executable(test_catalog tests/unit/test_catalog.c)
  target_link_libraries(test_catalog PRIVATE glk_core)
  target_include_directories(test_catalog PRIVATE ${GLK_INC})
  add_test(NAME catalog COMMAND test_catalog)
endif()

message(STATUS "GrokLink OS ${PROJECT_VERSION} — host=${GLK_PLATFORM_HOST}")
