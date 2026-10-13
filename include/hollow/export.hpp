#pragma once

/**
 * @file export.hpp
 * @brief Symbol visibility macros for building and consuming hollow as a
 *        static or shared library.
 *
 * Build-system contract (see src/CMakeLists.txt):
 *
 * - HOLLOW_SHARED            defined for the library and for every consumer
 *                           when hollow is built as a shared library;
 * - HOLLOW_BUILDING_LIBRARY  defined only while compiling hollow itself.
 *
 * With a static library neither macro is defined and HOLLOW_API expands to
 * nothing.
 */

#if defined(HOLLOW_SHARED)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(HOLLOW_BUILDING_LIBRARY)
#define HOLLOW_API __declspec(dllexport)
#else
#define HOLLOW_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define HOLLOW_API __attribute__((visibility("default")))
#else
#define HOLLOW_API
#endif
#else
#define HOLLOW_API
#endif
