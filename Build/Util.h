#ifndef UTIL_H
#define UTIL_H

#include <QObject>
#include <QDateTime>
#include <QFile>
#include <QString>
#include <QTextStream>

/* ---------------------------------------------------------------- */
/* Macros --------------------------------------------------------- */
/* ---------------------------------------------------------------- */

#define EPSILON 0.0000001

#ifndef MIN
#define MIN( a, b ) ((a) <= (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX( a, b ) ((a) >= (b) ? (a) : (b))
#endif

#define STR1(x) #x
#define STR(x) STR1(x)

#define STR2CHR( qstring )  ((qstring).toUtf8().constData())

#define STDSETTINGS( S, name )  \
    QSettings S( configPath( name ), QSettings::IniFormat )

/* ---------------------------------------------------------------- */
/* namespace Util ------------------------------------------------- */
/* ---------------------------------------------------------------- */

namespace Util
{

/* ---------------------------------------------------------------- */
/* Log messages to console ---------------------------------------- */
/* ---------------------------------------------------------------- */

void setLogFileName( QString name );

class Log
{
private:
    QTextStream stream;
protected:
    QString str;

public:
    Log();
    virtual ~Log();

    template <class T>
    Log &operator<<( const T &t ) {stream << t; return *this;}
};

#define Debug() Log()
#define Error() Log()

/* ---------------------------------------------------------------- */
/* Math ----------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// True if {a,b} closer than EPSILON (0.0000001)
bool feq( double a, double b );

// Uniform random deviate in range [rmin, rmax]
double uniformDev( double rmin = 0.0, double rmax = 1.0 );

// Position of least significant bit (like libc::ffs)
int ffs( int x );

/* ---------------------------------------------------------------- */
/* Objects -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void Connect(
    const QObject       *src,
    const QString       &sig,
    const QObject       *dst,
    const QString       &slot,
    Qt::ConnectionType  type = Qt::AutoConnection );

/* ---------------------------------------------------------------- */
/* Resources ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Convert resFile resource item to string
void res2Str( QString &str, const QString resFile );

// Remove terminal slash
QString rmvLastSlash( const QString &path );

// Current working directory
QString appPath();

// Full path to configs ini file
QString configPath( const QString &fileName );

// Full path to calibration folder
QString calibPath();

// Full path to calibration ini file
QString calibPath( const QString &fileName );

// Full path to tool item
bool toolPath( QString &path, const QString &toolName, bool bcreate );

/* ---------------------------------------------------------------- */
/* Timers --------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Thread-safe QDateTime::toString
QString dateTime2Str( const QDateTime &dt, Qt::DateFormat f = Qt::TextDate );
QString dateTime2Str( const QDateTime &dt, const QString &format );

// Current seconds from high resolution timer
double getTime();

/* ---------------------------------------------------------------- */
/* Execution environs --------------------------------------------- */
/* ---------------------------------------------------------------- */

// Set higher precision system timing on/off
void setPreciseTiming( bool on );

// Which processor calling thread is running on
int getCurProcessorIdx();

/* ---------------------------------------------------------------- */
/* end namespace Util --------------------------------------------- */
/* ---------------------------------------------------------------- */

}   // namespace Util

using namespace Util;

#endif  // UTIL_H


