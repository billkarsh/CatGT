
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* ---------------------------------------------------------------- */
/* Includes general ----------------------------------------------- */
/* ---------------------------------------------------------------- */

#include "Util.h"

/* ---------------------------------------------------------------- */
/* Includes single OS --------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <QDir>
#elif defined(Q_WS_X11)
    #include <GL/gl.h>
    #include <GL/glx.h>
    #include <X11/Xlib.h>
#elif defined(Q_WS_MACX)
    #include <agl.h>
    #include <gl.h>
#elif defined(Q_OS_LINUX)
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/sysinfo.h>
    #include <errno.h>
    #include <sched.h>
    #include <time.h>
    #include <unistd.h>
    #include <GL/gl.h>
#elif defined(Q_OS_DARWIN)
    #include <CoreServices/CoreServices.h>
    #include <GL/gl.h>
#endif

/* ---------------------------------------------------------------- */
/* Includes multi OS ---------------------------------------------- */
/* ---------------------------------------------------------------- */

#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    #include <QTime>
#endif

#if !defined(Q_OS_WIN)
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
#endif

/* ---------------------------------------------------------------- */
/* namespace Util ------------------------------------------------- */
/* ---------------------------------------------------------------- */

namespace Util {

/* ---------------------------------------------------------------- */
/* getTime -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef Q_OS_WIN

double getTime()
{
    static __int64  freq    = 0;
    static __int64  t0      = 0;
    __int64         tNow;

    QueryPerformanceCounter( (LARGE_INTEGER*)&tNow );

    if( !t0 )
        t0 = tNow;

    if( !freq )
        QueryPerformanceFrequency( (LARGE_INTEGER*)&freq );

    return double(tNow - t0) / double(freq);
}

#elif defined(Q_OS_LINUX)

double getTime()
{
    static double   t0 = -9999.;
    struct timespec ts;

    clock_gettime( CLOCK_MONOTONIC, &ts );

    double  t = double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;

    if( t0 < 0.0 )
        t0 = t;

    return t - t0;
}

#else /* !Q_OS_WIN && !Q_OS_LINUX */

double getTime()
{
    static QTime    t;
    static bool     started = false;

    if( !started ) {
        t.start();
        started = true;
    }

    return t.elapsed() / 1000.0;
}

#endif

/* ---------------------------------------------------------------- */
/* setPreciseTiming ----------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef Q_OS_WIN

void setPreciseTiming( bool on )
{
    if( on )
        timeBeginPeriod( 1 );
    else
        timeEndPeriod( 1 );
}

#else

void setPreciseTiming( bool on )
{
    Q_UNUSED( on )
}

#endif

/* ---------------------------------------------------------------- */
/* getCurProcessorIdx --------------------------------------------- */
/* ---------------------------------------------------------------- */

#ifdef Q_OS_WIN

int getCurProcessorIdx()
{
    return GetCurrentProcessorNumber();
}

#else

int getCurProcessorIdx()
{
    return 0;
}

#endif

/* ---------------------------------------------------------------- */
/* end namespace Util --------------------------------------------- */
/* ---------------------------------------------------------------- */

}   // namespace Util


