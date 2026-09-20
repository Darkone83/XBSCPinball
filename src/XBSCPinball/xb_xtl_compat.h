#pragma once

/*
 * xb_xtl_compat.h
 *
 * Compatibility wrapper for the legacy RXDK/XDK <xtl.h> headers when
 * compiling with modern MSVC.
 *
 * RXDK's WinBase.h declares these five legacy _Interlocked* functions with
 * extern "C" linkage. Modern MSVC already owns the same identifiers as
 * compiler intrinsics, which causes C2733 before any of our code is compiled.
 *
 * Do not redefine the intrinsics and do not modify RXDK headers. Instead,
 * hide ONLY the legacy RXDK declaration names while <xtl.h> is parsed.
 * Space Cadet's Xbox backend does not call these five functions.
 *
 * Xbox-only source files that need XDK types/APIs should include this header
 * instead of including <xtl.h> directly.
 */

#ifdef _MSC_VER

#pragma push_macro("_InterlockedIncrement")
#pragma push_macro("_InterlockedDecrement")
#pragma push_macro("_InterlockedExchange")
#pragma push_macro("_InterlockedExchangeAdd")
#pragma push_macro("_InterlockedCompareExchange")

#ifdef _InterlockedIncrement
#undef _InterlockedIncrement
#endif
#ifdef _InterlockedDecrement
#undef _InterlockedDecrement
#endif
#ifdef _InterlockedExchange
#undef _InterlockedExchange
#endif
#ifdef _InterlockedExchangeAdd
#undef _InterlockedExchangeAdd
#endif
#ifdef _InterlockedCompareExchange
#undef _InterlockedCompareExchange
#endif

 /*
  * Rename only the legacy declarations emitted while XTL/WinBase is parsed.
  * Once XTL has finished, the aliases are removed and MSVC's intrinsic names
  * are restored unchanged for normal C/C++ headers.
  */
#define _InterlockedIncrement        RXDK_Compat_InterlockedIncrement
#define _InterlockedDecrement        RXDK_Compat_InterlockedDecrement
#define _InterlockedExchange         RXDK_Compat_InterlockedExchange
#define _InterlockedExchangeAdd      RXDK_Compat_InterlockedExchangeAdd
#define _InterlockedCompareExchange  RXDK_Compat_InterlockedCompareExchange

#include <xtl.h>

#undef _InterlockedIncrement
#undef _InterlockedDecrement
#undef _InterlockedExchange
#undef _InterlockedExchangeAdd
#undef _InterlockedCompareExchange

#pragma pop_macro("_InterlockedCompareExchange")
#pragma pop_macro("_InterlockedExchangeAdd")
#pragma pop_macro("_InterlockedExchange")
#pragma pop_macro("_InterlockedDecrement")
#pragma pop_macro("_InterlockedIncrement")

#else

#include <xtl.h>

#endif
