cmake_minimum_required(VERSION 3.28)

project(blogin
  VERSION 0.9.1
  DESCRIPTION "An insanely fast static site generator written in C++"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Nothing here is a module. Leaving the scan on makes Ninja put a @...modmap
# argument in every compile command, which points at a file that only exists
# after a build, and clang-tidy reads those commands before anything is built.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(BLOGIN_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BLOGIN_SANITIZE_THREAD "Build with ThreadSanitizer" OFF)
option(BLOGIN_SANITIZE_INTEGER
  "Add the integer, local-bounds, and nullability checks to the sanitizer build" OFF)
option(BLOGIN_COVERAGE "Build with source-based coverage instrumentation" OFF)
option(BLOGIN_STATIC "Link the release binary as statically as the platform allows" OFF)
option(BLOGIN_FUZZ "Build the libFuzzer targets" OFF)
option(BLOGIN_TIDY "Run clang-tidy on every file the build compiles" OFF)

if(BLOGIN_SANITIZE AND BLOGIN_COVERAGE)
  message(FATAL_ERROR "BLOGIN_SANITIZE and BLOGIN_COVERAGE distort each other. Enable one at a time")
endif()

# ThreadSanitizer keeps its own shadow memory and cannot share a build with
# either of the others.
if(BLOGIN_SANITIZE_THREAD AND (BLOGIN_SANITIZE OR BLOGIN_COVERAGE))
  message(FATAL_ERROR "BLOGIN_SANITIZE_THREAD cannot be combined with BLOGIN_SANITIZE or BLOGIN_COVERAGE")
endif()

# Tidying during the build is the incremental way to work: only the files that
# changed are checked. scripts/tidy.sh checks all of them and is what CI runs.
if(BLOGIN_TIDY)
  find_program(BLOGIN_CLANG_TIDY NAMES clang-tidy HINTS /opt/homebrew/opt/llvm/bin REQUIRED)

  set(CMAKE_CXX_CLANG_TIDY "${BLOGIN_CLANG_TIDY}")

  # A clang-tidy from another toolchain, which on macOS it always is, has to be
  # told where the SDK headers live. CMAKE_OSX_SYSROOT is only filled in when
  # the build itself needs it, so the SDK path is asked for when it is empty.
  if(APPLE)
    set(blogin_sysroot "${CMAKE_OSX_SYSROOT}")

    if(NOT blogin_sysroot)
      execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE blogin_sysroot
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
      )
    endif()

    list(APPEND CMAKE_CXX_CLANG_TIDY "--extra-arg=-isysroot${blogin_sysroot}")
  endif()
endif()

# Every target's warning, sanitizer, and coverage settings arrive through this
# one interface target, so a new target cannot quietly opt out.
add_library(blogin_flags INTERFACE)

target_compile_features(blogin_flags INTERFACE cxx_std_23)

# The version the binary reports comes from project() above, so `blogin
# --version` and the tag a release is cut from cannot disagree by accident.
# scripts/release.sh checks the tag against this.
target_compile_definitions(blogin_flags INTERFACE BLOGIN_VERSION="${PROJECT_VERSION}")

target_compile_options(blogin_flags INTERFACE
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -Wshadow
  -Wconversion
  -Wsign-conversion
  -Wnon-virtual-dtor
  -Wold-style-cast
  -Wcast-align
  -Wunused
  -Woverloaded-virtual
  -Wdouble-promotion
  -Wformat=2
  -Wimplicit-fallthrough
  -Wextra-semi
  -Wsuggest-override
)

# -Wfloat-equal is left off. Every exact comparison this codebase makes is one
# it means: the expression language's own == operator, the <=> behind it, the
# tie-break in the page sort, and the negative-zero check in the JSON writer.
# The warning found no defect and would cost four suppressions, which is the
# kind of finding .clang-tidy already turns off by policy.

# Hardening.
#
# The sanitizers only protect a run that is built with them, which is CI and a
# developer's machine. Everything below protects the binary a reader downloads.
# Several of the flags exist on one platform and not another, so each is probed
# rather than assumed, and a compiler that rejects one simply does not get it.
include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)
include(CheckLinkerFlag)

function(blogin_try_compile_option flag)
  string(MAKE_C_IDENTIFIER "blogin_accepts${flag}" probe)

  set(CMAKE_REQUIRED_FLAGS -Werror)
  check_cxx_compiler_flag("${flag}" ${probe})

  if(${probe})
    target_compile_options(blogin_flags INTERFACE "${flag}")
    set_property(GLOBAL APPEND PROPERTY blogin_hardening_flags "${flag}")
  endif()
endfunction()

function(blogin_try_link_option flag)
  string(MAKE_C_IDENTIFIER "blogin_links${flag}" probe)

  check_linker_flag(CXX "${flag}" ${probe})

  if(${probe})
    target_link_options(blogin_flags INTERFACE "${flag}")
  endif()
endfunction()

# Bounds-checks the standard library containers: vector and span subscripting,
# string_view indexing, and iterator ranges. Extensive is the mode meant for a
# tested build, fast is the one meant to ship, and fast costs about one percent.
# Whichever standard library is in use ignores the define meant for the other.
target_compile_definitions(blogin_flags INTERFACE
  $<IF:$<CONFIG:Release>,
    _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST,
    _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE>
  _GLIBCXX_ASSERTIONS
)

# Reading an uninitialized local stops being undefined and starts being a read
# of a known value. Release fills with zero, which is the cheaper of the two;
# every other configuration fills with 0xAA, so a stale pointer dereference
# lands somewhere obvious rather than at null.
blogin_try_compile_option(-ftrivial-auto-var-init=pattern)

if(blogin_accepts_ftrivial_auto_var_init_pattern)
  target_compile_options(blogin_flags INTERFACE
    $<$<CONFIG:Release>:-ftrivial-auto-var-init=zero>)
endif()

blogin_try_compile_option(-fstack-protector-strong)

# Neither of these exists on arm64 macOS. The probe sorts that out.
blogin_try_compile_option(-fstack-clash-protection)
blogin_try_compile_option(-fcf-protection=full)

# Return-address signing, which arm64 macOS accepts and then cannot unwind
# through: the specs build clean with it and abort on the first throw, because
# the system unwinder does not expect signed return addresses outside arm64e.
# Accepting a flag is not the same as running with it, so this one is asked for
# only where it is known to work.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  blogin_try_compile_option(-mbranch-protection=standard)
endif()

# Relocations read-only and bound at load time, and a stack the kernel will not
# execute. ELF only, so macOS skips all three.
blogin_try_link_option(LINKER:-z,relro)
blogin_try_link_option(LINKER:-z,now)
blogin_try_link_option(LINKER:-z,noexecstack)

# The fortified libc entry points need the optimizer to know the buffer sizes,
# so this is meaningless in a Debug build and warns when asked for anyway. It
# also duplicates what the sanitizers already intercept, and the two disagree.
if(NOT BLOGIN_SANITIZE AND NOT BLOGIN_SANITIZE_THREAD)
  target_compile_options(blogin_flags INTERFACE
    $<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE>
    $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>
  )
endif()

# Every flag above was accepted by the compiler, which is a weaker statement
# than the binary working. Return-address signing is accepted on arm64 macOS and
# then aborts on the first throw, because the system unwinder does not expect
# signed return addresses outside arm64e. Throwing and catching here exercises
# the unwinder at configure time, so a flag that breaks it is caught before it
# reaches a spec run or a release.
get_property(blogin_hardening GLOBAL PROPERTY blogin_hardening_flags)

if(blogin_hardening AND NOT CMAKE_CROSSCOMPILING)
  string(REPLACE ";" " " CMAKE_REQUIRED_FLAGS "${blogin_hardening}")

  check_cxx_source_runs("
    struct Boom {};

    int main() {
      try {
        throw Boom{};
      } catch (const Boom&) {
        return 0;
      }

      return 1;
    }" blogin_hardening_unwinds)

  if(NOT blogin_hardening_unwinds)
    message(FATAL_ERROR
      "the hardening flags (${blogin_hardening}) produce a binary that cannot unwind an exception. "
      "Drop the platform-specific ones one at a time to find which.")
  endif()
endif()

if(BLOGIN_SANITIZE)
  target_compile_options(blogin_flags INTERFACE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all
  )
  target_link_options(blogin_flags INTERFACE -fsanitize=address,undefined)

  # Checks the default undefined set leaves out, because what they catch is
  # defined behaviour that is nearly always a mistake rather than undefined
  # behaviour outright.
  #
  #   integer          unsigned wraparound, and a conversion that silently
  #                    truncates or changes sign. The parsers do offset
  #                    arithmetic on size_t, where wrapping is how a length
  #                    computation turns into an enormous read.
  #   local-bounds     a stack array indexed past its end, which the address
  #                    sanitizer's redzones do not always sit next to.
  #   nullability      a null passed where an annotation says it cannot be.
  if(BLOGIN_SANITIZE_INTEGER)
    target_compile_options(blogin_flags INTERFACE
      -fsanitize=integer,local-bounds,nullability
      "-fsanitize-ignorelist=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizer-ignorelist.txt"
    )
    target_link_options(blogin_flags INTERFACE -fsanitize=integer,local-bounds,nullability)
  endif()
endif()

if(BLOGIN_SANITIZE_INTEGER AND NOT BLOGIN_SANITIZE)
  message(FATAL_ERROR "BLOGIN_SANITIZE_INTEGER adds to the sanitizer build. Enable BLOGIN_SANITIZE too")
endif()

# GCC builds and tests everything else, including AddressSanitizer,
# UndefinedBehaviorSanitizer, and ThreadSanitizer. These two are clang's alone.
# Caught here rather than left to the compiler, which otherwise reports
# `unrecognized argument to -fsanitize=` on the first file of a long build.
if(BLOGIN_SANITIZE_INTEGER AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "BLOGIN_SANITIZE_INTEGER needs clang: the integer, local-bounds, and "
    "nullability checks and -fsanitize-ignorelist are clang's own. "
    "Configure with -DCMAKE_CXX_COMPILER=clang++, or leave this option off.")
endif()

if(BLOGIN_COVERAGE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR
    "BLOGIN_COVERAGE needs clang: the instrumentation is source-based coverage, "
    "and scripts/coverage.sh reads it with llvm-profdata and llvm-cov. "
    "Configure with -DCMAKE_CXX_COMPILER=clang++, or leave this option off.")
endif()

if(BLOGIN_SANITIZE_THREAD)
  target_compile_options(blogin_flags INTERFACE
    -fsanitize=thread
    -fno-omit-frame-pointer
    -g
  )
  target_link_options(blogin_flags INTERFACE -fsanitize=thread)
endif()

if(BLOGIN_COVERAGE)
  target_compile_options(blogin_flags INTERFACE -fprofile-instr-generate -fcoverage-mapping)
  target_link_options(blogin_flags INTERFACE -fprofile-instr-generate -fcoverage-mapping)
endif()

add_library(blogin_core
  src/blogin/counters.cpp
  src/blogin/value.cpp
  src/blogin/text.cpp
  src/blogin/date.cpp
  src/blogin/files.cpp
  src/blogin/json.cpp
  src/blogin/yaml.cpp
  src/blogin/config.cpp
  src/blogin/data.cpp
  src/blogin/slug.cpp
  src/blogin/metrics.cpp
  src/blogin/summary.cpp
  src/blogin/filters.cpp
  src/blogin/toc.cpp
  src/blogin/framework.cpp
  src/blogin/highlight.cpp
  src/blogin/shortcode.cpp
  src/blogin/style.cpp
  src/blogin/assets.cpp
  src/blogin/scaffold.cpp
  src/blogin/cli.cpp
  src/blogin/http.cpp
  src/blogin/websocket.cpp
  src/blogin/reload.cpp
  src/blogin/watcher.cpp
  src/blogin/server.cpp
  src/blogin/post.cpp
  src/blogin/nav.cpp
  src/blogin/feed.cpp
  src/blogin/search.cpp
  src/blogin/expression.cpp
  src/blogin/view_context.cpp
  src/blogin/haml.cpp
  src/blogin/haml_render.cpp
  src/blogin/template_store.cpp
  src/blogin/view.cpp
  src/blogin/view_theme.cpp
  src/blogin/writer.cpp
  src/blogin/build_state.cpp
  src/blogin/site.cpp
  src/blogin/log.cpp
  src/blogin/markdown.cpp
  src/blogin/markdown_shared.cpp
  src/blogin/markdown_inline.cpp
  src/blogin/html.cpp
  src/blogin/template.cpp
  src/blogin/build.cpp
)

target_include_directories(blogin_core PUBLIC src/blogin/include)
target_link_libraries(blogin_core PUBLIC blogin_flags)

find_package(Threads REQUIRED)
target_link_libraries(blogin_core PUBLIC Threads::Threads)

# FSEvents, for watching files on macOS. A system framework rather than a
# dependency: nothing to install and nothing to vendor.
if(APPLE)
  target_link_libraries(blogin_core PUBLIC "-framework CoreServices")
endif()

add_executable(blogin src/main.cpp)
target_link_libraries(blogin PRIVATE blogin_core)

# A downloaded binary should run without installing a runtime. Linux can be
# fully static. macOS cannot: Apple ships no static libc, so every binary on the
# platform links libSystem, and the most that can be done is to stop depending
# on whichever libc++ the user happens to have.
if(BLOGIN_STATIC)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(blogin PRIVATE -static)
  elseif(APPLE)
    target_link_options(blogin PRIVATE -nostdlib++)
    target_link_libraries(blogin PRIVATE -lc++ -lc++abi)
  endif()

  set_property(TARGET blogin PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_executable(blogin_bench src/bench.cpp)
target_link_libraries(blogin_bench PRIVATE blogin_core)

install(TARGETS blogin RUNTIME DESTINATION bin)

enable_testing()

add_executable(blogin_specs
  specs/spec_main.cpp
  specs/support/spec.cpp
  specs/support/golden.cpp
  specs/arena_spec.cpp
  specs/framework_spec.cpp
  specs/markdown_spec.cpp
  specs/template_spec.cpp
  specs/build_spec.cpp
  specs/concurrency_spec.cpp
  specs/generated_spec.cpp
  specs/counters_spec.cpp
  specs/corpus_spec.cpp
  specs/value_spec.cpp
  specs/text_spec.cpp
  specs/date_spec.cpp
  specs/files_spec.cpp
  specs/json_spec.cpp
  specs/yaml_spec.cpp
  specs/config_spec.cpp
  specs/data_spec.cpp
  specs/markdown_extensions_spec.cpp
  specs/rendering_spec.cpp
  specs/post_spec.cpp
  specs/expression_spec.cpp
  specs/haml_spec.cpp
  specs/haml_corpus_spec.cpp
  specs/fragment_spec.cpp
  specs/view_spec.cpp
  specs/view_context_spec.cpp
  specs/writer_spec.cpp
  specs/site_spec.cpp
  specs/acceptance_spec.cpp
  specs/assets_spec.cpp
  specs/asset_pipeline_spec.cpp
  specs/scaffold_spec.cpp
  specs/cli_spec.cpp
  specs/http_spec.cpp
  specs/websocket_spec.cpp
  specs/watcher_spec.cpp
  specs/server_spec.cpp
  specs/theme_spec.cpp
  specs/language_spec.cpp
  specs/nav_spec.cpp
  specs/feed_spec.cpp
  specs/search_spec.cpp
  specs/commonmark_spec.cpp
  specs/support/commonmark.cpp
  specs/log_spec.cpp
  specs/hardening_spec.cpp
  specs/spec_harness_spec.cpp
)

target_include_directories(blogin_specs PRIVATE specs)
target_link_libraries(blogin_specs PRIVATE blogin_core)

target_compile_definitions(blogin_specs PRIVATE
  BLOGIN_SPECS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/specs"
  BLOGIN_COMMONMARK_FLOOR=620
)

add_test(NAME specs COMMAND blogin_specs)

# One target per parser that reads bytes nobody on this side wrote. Adding a
# parser without adding a target here leaves it fuzzed by nothing.
set(BLOGIN_FUZZ_TARGETS
  markdown
  template
  haml
  json
  yaml
  config
  expression
  shortcode
  http
  path
  websocket
  text
)

if(BLOGIN_FUZZ)
  foreach(fuzz_target IN LISTS BLOGIN_FUZZ_TARGETS)
    add_executable(fuzz_${fuzz_target} fuzz/${fuzz_target}_fuzz.cpp)

    target_include_directories(fuzz_${fuzz_target} PRIVATE fuzz)
    target_link_libraries(fuzz_${fuzz_target} PRIVATE blogin_core)
    target_compile_options(fuzz_${fuzz_target} PRIVATE -fsanitize=fuzzer)
    target_link_options(fuzz_${fuzz_target} PRIVATE -fsanitize=fuzzer)
  endforeach()
endif()
