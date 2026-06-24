# Copyright (c) 2019 The STE||AR-Group
#
# SPDX-License-Identifier: BSL-1.0
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(benchmarks future_boundary)

set(future_boundary_PARAMETERS THREADS_PER_LOCALITY 4)

foreach(benchmark ${benchmarks})
  set(sources ${benchmark}.cpp)

  source_group("Source Files" FILES ${sources})

  add_hpx_executable(
    ${benchmark}_test INTERNAL_FLAGS
    SOURCES ${sources}
    EXCLUDE_FROM_ALL
    DEPENDENCIES hpx_execution
    FOLDER "Benchmarks/Modules/Core/Execution"
  )

  add_hpx_performance_test("modules.execution" ${benchmark}
    ${${benchmark}_PARAMETERS}
  )
endforeach()
