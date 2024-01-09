#ifndef __CPL_DEBUG_H__
#define __CPL_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdarg.h"
#include "stdio.h"

static inline int Pr(const char *file, int line, const char *func, const char *errStr,
                 const char *fmt, ...)
{
    char buf[1024] = {0};
    va_list ap;
    int ret;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    ret = printf("%s %05d %s [%s] %s\n", file, line, func, errStr, buf);
    return ret;
}

#ifdef __cplusplus
}
#endif

#endif /*__CPL_DEBUG_H__*/
