#pragma once

#include <stdint.h>

//////////////////////////////////////////////////////////////////////////
#if defined(_DEBUG) || defined(FORCE_DEBUG)
#  define PLATFORM_IS_DEBUG 1
#else
#  define PLATFORM_IS_NDEBUG 1
#endif

#if defined(_WIN32)
#  include <intrin.h>
#  define PLATFORM_IS_WINDOWS 1
#  if defined(_WIN64)
#    define PLATFORM_IS_WIN64 1
#  endif
#endif

#if defined(__ANDROID__)
#  define PLATFORM_IS_ANDROID 1
#endif

#if defined(__APPLE__)
#  define PLATFORM_IS_APPLE 1
#  include <TargetConditionals.h>
#  if TARGET_IPHONE_SIMULATOR
#    define PLATFORM_IS_IPHONE 1
#    define PLATFORM_IS_IPHONE_SIMULATOR 1
#  elif TARGET_OS_IPHONE
#    define PLATFORM_IS_IPHONE 1
#  elif TARGET_OS_MAC
#    define PLATFORM_IS_MAC 1
#  endif
#endif

#if defined(__linux__)
#  define PLATFORM_IS_LINUX 1
#endif

#if defined(__FreeBSD__)
#  define PLATFORM_IS_FREEBSD 1
#endif

#if defined(__unix__)
#  define PLATFORM_IS_UNIX 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  define PLATFORM_ARCH_X86 1
#  define PLATFORM_IS_X64 1
#elif defined(__i386__) || defined(_M_IX86)
#  define PLATFORM_ARCH_X86 1
#  define PLATFORM_IS_X86 1
#endif

#if defined(__aarch64__)
#  define PLATFORM_ARCH_ARM 1
#  define PLATFORM_IS_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
#  define PLATFORM_ARCH_ARM 1
#  define PLATFORM_IS_ARM32 1
#endif

//////////////////////////////////////////////////////////////////////////
#if UINTPTR_MAX == UINT32_MAX
#  define PLATFORM_IS_32BIT 1
#else
#  define PLATFORM_IS_64BIT 1
#endif

//////////////////////////////////////////////////////////////////////////
#if defined(__clang__) || defined(__GNUC__)
#  define PLATFORM_GNUC_LIKE 1
#  if defined(PLATFORM_ARCH_X86)
#    include <x86intrin.h>
#  elif defined(PLATFORM_ARCH_ARM)
#    include <arm_acle.h>
#    if defined(__ARM_NEON__)
#      include <arm_neon.h>
#    endif
#  endif
#elif defined(_MSC_VER)
#  define PLATFORM_MSVC_LIKE 1
#else
#  error Cannot detect compiler environment!
#endif

//////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_GNUC_LIKE)
#  define PLATFORM_LIKELY(x)			__builtin_expect(!!(x), 1)
#  define PLATFORM_UNLIKELY(x)			__builtin_expect(!!(x), 0)
#  define PLATFORM_FORCE_INLINE			inline __attribute__((always_inline))
#  define PLATFORM_NO_INLINE			__attribute__((noinline))
#  define PLATFORM_MAYBE_UNUSED			__attribute__((unused))
#  define PLATFORM_WARN_UNUSED			__attribute__((warn_unused_result))
#  define PLATFORM_PRINTF_LIKE(f, a)	__attribute__((format(printf, f, a)))
#else
#  define PLATFORM_LIKELY(x)			x
#  define PLATFORM_UNLIKELY(x)			x
#  define PLATFORM_FORCE_INLINE			__forceinline
#  define PLATFORM_NO_INLINE			__declspec(noinline)
#  define PLATFORM_MAYBE_UNUSED
#  define PLATFORM_WARN_UNUSED
#  define PLATFORM_PRINTF_LIKE(f, a)
#endif

//////////////////////////////////////////////////////////////////////////
#if defined(PLATFORM_IS_WINDOWS)

typedef intptr_t ssize_t;

int gettimeofday(struct timeval *tp, struct timezone *tzp);

#endif

//////////////////////////////////////////////////////////////////////////
#define CONFIG_VERSION "2020-07-05"
