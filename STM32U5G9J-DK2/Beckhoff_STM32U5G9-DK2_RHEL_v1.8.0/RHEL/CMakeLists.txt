# @file CMakeLists.txt
# @brief Single production build for one EK9500 Modbus TCP gateway.
# @author Prathamesh Chipkar
cmake_minimum_required(VERSION 3.20)
project(ek9500_modbus_tcp_gateway VERSION 1.8.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_definitions(_POSIX_C_SOURCE=200809L)

find_package(Threads REQUIRED)

add_library(project_warnings INTERFACE)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(project_warnings INTERFACE
    -O2 -g
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wsign-conversion -Wshadow -Wformat=2
    -Wundef -Wcast-align -Wstrict-prototypes -Wmissing-prototypes
    -Wold-style-definition -Wpointer-arith -Wwrite-strings)
endif()

set(EK9500_NATIVE_SOURCES
  src/core/types.c
  src/core/logger.c
  src/core/config.c
  src/core/layout.c
  src/core/process_image.c
  src/core/terminal_manager.c
  src/terminals/el1409.c
  src/terminals/el2409.c
  src/terminals/el3074.c
  src/terminals/el4074.c
  src/eip/eip_session.c
  src/eip/cip.c
  src/eip/cip_explicit.c
  src/eip/cip_connection.c
  src/eip/eip_transport.c
)

add_library(ek9500_native STATIC ${EK9500_NATIVE_SOURCES})
target_include_directories(ek9500_native PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/eip)
target_link_libraries(ek9500_native PUBLIC Threads::Threads m PRIVATE project_warnings)
target_compile_definitions(ek9500_native PUBLIC EAD_VERSION="${PROJECT_VERSION}")

add_library(gateway_core STATIC
  src/gateway/gateway_types.c
  src/gateway/gateway_config.c
  src/gateway/route_manager.c)
target_include_directories(gateway_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(gateway_core PUBLIC ek9500_native Threads::Threads m PRIVATE project_warnings)

add_library(modbus_tcp STATIC
  src/modbus_tcp/modbus_master_acl.c
  src/modbus_tcp/modbus_data_model.c
  src/modbus_tcp/modbus_coils.c
  src/modbus_tcp/modbus_discrete_inputs.c
  src/modbus_tcp/modbus_holding_registers.c
  src/modbus_tcp/modbus_input_registers.c
  src/modbus_tcp/modbus_tcp_protocol.c
  src/modbus_tcp/modbus_tcp_server.c)
target_include_directories(modbus_tcp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(modbus_tcp PUBLIC gateway_core PRIVATE project_warnings)

add_executable(ek9500_modbus_tcp_gateway app/main.c)
target_link_libraries(ek9500_modbus_tcp_gateway PRIVATE modbus_tcp project_warnings)
target_compile_definitions(ek9500_modbus_tcp_gateway PRIVATE
  GW_DEFAULT_CONFIG_PATH="config/gateway.conf"
  GW_VERSION="${PROJECT_VERSION}")

enable_testing()
add_executable(test_configuration tests/test_configuration.c)
target_link_libraries(test_configuration PRIVATE modbus_tcp project_warnings)
target_compile_definitions(test_configuration PRIVATE
  TEST_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config")
add_test(NAME configuration COMMAND test_configuration)

add_executable(test_modbus_protocol tests/test_modbus_protocol.c)
target_link_libraries(test_modbus_protocol PRIVATE modbus_tcp project_warnings)
target_compile_definitions(test_modbus_protocol PRIVATE
  TEST_GATEWAY_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/config/gateway.conf")
add_test(NAME modbus_protocol COMMAND test_modbus_protocol)

add_executable(test_modbus_multi_client tests/test_modbus_multi_client.c)
target_link_libraries(test_modbus_multi_client PRIVATE modbus_tcp project_warnings)
target_compile_definitions(test_modbus_multi_client PRIVATE
  TEST_GATEWAY_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/config/gateway.conf")
add_test(NAME modbus_multi_client COMMAND test_modbus_multi_client)

add_executable(test_master_acl tests/test_master_acl.c)
target_link_libraries(test_master_acl PRIVATE modbus_tcp project_warnings)
add_test(NAME master_acl COMMAND test_master_acl)

add_executable(test_master_access_control tests/test_master_access_control.c)
target_link_libraries(test_master_access_control PRIVATE modbus_tcp project_warnings)
target_compile_definitions(test_master_access_control PRIVATE
  TEST_GATEWAY_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/config/gateway.conf")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  # GCC analyzer incorrectly flags the valid bind(source-IP)-then-connect test socket.
  target_compile_options(test_master_access_control PRIVATE
    -Wno-analyzer-fd-phase-mismatch)
endif()
add_test(NAME master_access_control COMMAND test_master_access_control)

add_executable(test_dual_listener tests/test_dual_listener.c)
target_link_libraries(test_dual_listener PRIVATE modbus_tcp project_warnings)
target_compile_definitions(test_dual_listener PRIVATE
  TEST_GATEWAY_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/config/gateway.conf")
add_test(NAME dual_listener COMMAND test_dual_listener)

add_executable(test_ao_scaling tests/test_ao_scaling.c)
target_link_libraries(test_ao_scaling PRIVATE modbus_tcp project_warnings)
add_test(NAME ao_scaling COMMAND test_ao_scaling)

add_test(NAME masterctl_stm32_profile
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_masterctl.sh)

add_executable(test_master_rate_limit tests/test_master_rate_limit.c)
target_link_libraries(test_master_rate_limit PRIVATE modbus_tcp project_warnings)
target_compile_definitions(test_master_rate_limit PRIVATE
  TEST_GATEWAY_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/config/gateway.conf")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(test_master_rate_limit PRIVATE
    -Wno-analyzer-fd-phase-mismatch)
endif()
add_test(NAME master_rate_limit COMMAND test_master_rate_limit)


find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Doxygen documentation")
endif()

install(TARGETS ek9500_modbus_tcp_gateway RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION /etc/ek9500-modbus-gateway)
install(FILES systemd/gateway.conf DESTINATION /etc/ek9500-modbus-gateway RENAME gateway.conf)
install(PROGRAMS scripts/masterctl.sh DESTINATION sbin RENAME ek9500-masterctl)
install(PROGRAMS tools/ek9500_modbus_live_monitor.py DESTINATION bin RENAME ek9500-modbus-live-monitor)
install(PROGRAMS tools/stm32_f407_lan8720_contract_test.py DESTINATION bin RENAME ek9500-stm32-contract-test)
install(PROGRAMS tools/modbus_cli.py DESTINATION bin RENAME ek9500-modbus-cli)
install(FILES README.md CHANGELOG.md TEST_REPORT.md THIRD_PARTY.md DESTINATION share/doc/ek9500-modbus-gateway)
install(DIRECTORY docs/ DESTINATION share/doc/ek9500-modbus-gateway/docs
        FILES_MATCHING PATTERN "*.md" PATTERN "*.dox")
install(FILES systemd/ek9500-modbus-tcp-gateway.service DESTINATION /etc/systemd/system)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME network_integration COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_integration.py $<TARGET_FILE:ek9500_modbus_tcp_gateway>)
  set_tests_properties(network_integration PROPERTIES TIMEOUT 90)
endif()
