cmake_minimum_required(VERSION 3.16)

project(industrial_dpdk_rx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(
    -Wall
    -Wextra
    -mssse3
)

# -----------------------------------------------------------------------------
# DPDK root
# -----------------------------------------------------------------------------
set(DPDK_ROOT "$ENV{DPDK_ROOT}" CACHE PATH "DPDK root directory")
if(NOT DPDK_ROOT)
    set(DPDK_ROOT "${PROJECT_SOURCE_DIR}/thirdparty/UNIX/lib/DPDK")
endif()

if(NOT EXISTS "${DPDK_ROOT}")
    message(FATAL_ERROR "DPDK root not found: ${DPDK_ROOT}")
endif()

message(STATUS "DPDK root: ${DPDK_ROOT}")

# -----------------------------------------------------------------------------
# Find DPDK headers
# -----------------------------------------------------------------------------
find_path(DPDK_INCLUDE_DIR
    NAMES rte_eal.h
    PATHS
        "${DPDK_ROOT}/include"
        "${DPDK_ROOT}/include/dpdk"
        "${DPDK_ROOT}/src/lib"
    NO_DEFAULT_PATH
)

if(NOT DPDK_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find rte_eal.h under ${DPDK_ROOT}")
endif()

message(STATUS "DPDK include dir: ${DPDK_INCLUDE_DIR}")

# -----------------------------------------------------------------------------
# Search DPDK libraries
# -----------------------------------------------------------------------------
set(DPDK_SEARCH_DIRS
    "${DPDK_ROOT}"
    "${DPDK_ROOT}/lib"
    "${DPDK_ROOT}/lib64"
    "${DPDK_ROOT}/lib/x86_64-linux-gnu"
    "${DPDK_ROOT}/lib64/x86_64-linux-gnu"
    "${DPDK_ROOT}/build/lib"
    "${DPDK_ROOT}/build/lib64"
)

set(DPDK_REQUIRED_LIBS
    rte_acl
    rte_bbdev
    rte_bitratestats
    rte_bpf
    rte_cfgfile
    rte_cmdline
    rte_compressdev
    rte_cryptodev
    rte_distributor
    rte_efd
    rte_ethdev
    rte_eventdev
    rte_fib
    rte_flow_classify
    rte_graph
    rte_gro
    rte_gso
    rte_hash
    rte_ip_frag
    rte_ipsec
    rte_jobstats
    rte_kni
    rte_kvargs
    rte_latencystats
    rte_lpm
    rte_mbuf
    rte_member
    rte_mempool
    rte_meter
    rte_metrics
    rte_net
    rte_node
    rte_pci
    rte_pdump
    rte_pipeline
    rte_port
    rte_power
    rte_rawdev
    rte_rcu
    rte_regexdev
    rte_reorder
    rte_rib
    rte_ring
    rte_sched
    rte_security
    rte_stack
    rte_table
    rte_telemetry
    rte_timer
    rte_vhost
    rte_eal
)

set(DPDK_ABSOLUTE_LIBS "")
foreach(libname IN LISTS DPDK_REQUIRED_LIBS)
    find_library(_lib
        NAMES "${libname}" "lib${libname}"
        PATHS ${DPDK_SEARCH_DIRS}
        NO_DEFAULT_PATH
    )

    if(_lib)
        list(APPEND DPDK_ABSOLUTE_LIBS "${_lib}")
        message(STATUS "Found DPDK lib: ${_lib}")
    else()
        message(WARNING "DPDK lib not found: ${libname}")
    endif()
endforeach()

if(NOT DPDK_ABSOLUTE_LIBS)
    message(FATAL_ERROR "Could not find any DPDK shared libraries under ${DPDK_ROOT}")
endif()

# -----------------------------------------------------------------------------
# DPDK interface target
# -----------------------------------------------------------------------------
add_library(dpdk_iface INTERFACE)
target_include_directories(dpdk_iface
    INTERFACE
        "${DPDK_INCLUDE_DIR}"
        "${DPDK_ROOT}/include"
        "${DPDK_ROOT}/include/dpdk"
)

target_link_directories(dpdk_iface
    INTERFACE
        ${DPDK_SEARCH_DIRS}
)

target_link_libraries(dpdk_iface
    INTERFACE
        -Wl,--no-as-needed
        ${DPDK_ABSOLUTE_LIBS}
        -Wl,--as-needed
        pthread
        numa
        rt
        dl
        m
        atomic
)

# -----------------------------------------------------------------------------
# Project library
# -----------------------------------------------------------------------------
add_library(industrial_rx SHARED
    src/protocol.cpp
    src/mempool.cpp
    src/port.cpp
    src/wal.cpp
    src/primary.cpp
    src/worker.cpp
    src/tx.cpp
)

target_include_directories(industrial_rx
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${DPDK_INCLUDE_DIR}"
        "${DPDK_ROOT}/include"
        "${DPDK_ROOT}/include/dpdk"
)

target_link_libraries(industrial_rx
    PUBLIC
        dpdk_iface
)

# -----------------------------------------------------------------------------
# Executables
# -----------------------------------------------------------------------------
add_executable(rx_primary
    apps/rx_primary_main.cpp
)

target_link_libraries(rx_primary
    PRIVATE
        industrial_rx
)

add_executable(channel_worker
    apps/channel_worker_main.cpp
)

target_link_libraries(channel_worker
    PRIVATE
        industrial_rx
)

# -----------------------------------------------------------------------------
# Runtime library path
# -----------------------------------------------------------------------------
set(DPDK_RUNTIME_DIR "")
foreach(_lib IN LISTS DPDK_ABSOLUTE_LIBS)
    get_filename_component(_dir "${_lib}" DIRECTORY)
    set(DPDK_RUNTIME_DIR "${_dir}")
    break()
endforeach()

if(DPDK_RUNTIME_DIR)
    set_target_properties(industrial_rx rx_primary channel_worker
        PROPERTIES
            BUILD_RPATH "${DPDK_RUNTIME_DIR}"
            INSTALL_RPATH "${DPDK_RUNTIME_DIR}"
    )
endif()
