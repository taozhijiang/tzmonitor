#ifndef __TZ_TIMER_SERVICE__
#define __TZ_TIMER_SERVICE__

#if defined(BUILD_VERSION_V1)

	#include "detail/TSv1.h"
	
#elif defined(BUILD_VERSION_V2)

	#include "detail/TSv2.h"
	
#else
	
	#error "BUILD_VERSION unspecified"
	
#endif

#endif // __TZ_TIMER_SERVICE__
