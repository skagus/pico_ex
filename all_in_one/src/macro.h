#pragma once
/****************************************************************************

****************************************************************************/
#include <stdio.h>
#include "types.h"

#define NOT(x)							(!(x))
#define PRINT_DEF(x)					PRINTF("%s:%d\n", #x, x)

/// Build time assert.
#define BUILD_IF(cond)					typedef char ttteas[(cond)?1:-1]
#define ASSERT(cond)					if(!(cond)){printf("ASSERT:%s(%d) %s\n", __FILE__, __LINE__, #cond); while(1);}

#define BIT(x)							(1 << (x))
#define BIT_SET(val, off)				(val) |= BIT(off)
#define BIT_CLR(val, off)				(val) &= ~BIT(off)

#define DIV_DN(x, unit)					((x) / (unit))	   ///< div to lower bound.
#define DIV_UP(x, unit)					(((x)+(unit)-1) / (unit))	   ///< div to upper bound.
