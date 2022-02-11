
#include "Util.h"

#include <iostream>

#include <QMessageBox>
#include <QThread>
#include <QMutex>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>


/* ---------------------------------------------------------------- */
/* namespace Util ------------------------------------------------- */
/* ---------------------------------------------------------------- */

namespace Util {

/* ---------------------------------------------------------------- */
/* Log messages to console ---------------------------------------- */
/* ---------------------------------------------------------------- */

static QString  logName;


void setLogFileName( QString name )
{
    logName = name;
}


Log::Log() : stream( &str, QIODevice::WriteOnly )
{
}


Log::~Log()
{
    QString msg =
        QString("[Thd %1 CPU %2 %3] %4")
            .arg( quint64(QThread::currentThreadId()) )
            .arg( getCurProcessorIdx() )
            .arg( dateTime2Str(
                    QDateTime::currentDateTime(),
                    "M/dd/yy hh:mm:ss.zzz" ) )
            .arg( str );

    QFile f( logName );
    f.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text );
    QTextStream ts( &f );

    ts << msg << "\n";
}

/* ---------------------------------------------------------------- */
/* Math ----------------------------------------------------------- */
/* ---------------------------------------------------------------- */

bool feq( double a, double b )
{
    double  diff = a - b;
    return (diff < EPSILON) && (-diff < EPSILON);
}


double uniformDev( double rmin, double rmax )
{
    static bool seeded = false;

    if( !seeded ) {
        seeded = true;
        qsrand( std::time(0) );
    }

    return rmin + (rmax-rmin) * qrand() / RAND_MAX;
}


// Bit position of LSB( X ).
// Right-most bit is bit-0.
// E.g. ffs( 0x8 ) = 4.
//
int ffs( int x )
{
    int r = 1;

    if( !x )
        return 0;

    if( !(x & 0xffff) ) {
        x >>= 16;
        r += 16;
    }

    if( !(x & 0xff) ) {
        x >>= 8;
        r += 8;
    }

    if( !(x & 0xf) ) {
        x >>= 4;
        r += 4;
    }

    if( !(x & 3) ) {
        x >>= 2;
        r += 2;
    }

    if( !(x & 1) ) {
//        x >>= 1;
        r += 1;
    }

    return r;
}

/* ---------------------------------------------------------------- */
/* Objects -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void Connect(
    const QObject       *src,
    const QString       &sig,
    const QObject       *dst,
    const QString       &slot,
    Qt::ConnectionType  type )
{
    Q_UNUSED( src )
    Q_UNUSED( sig )
    Q_UNUSED( dst )
    Q_UNUSED( slot )
    Q_UNUSED( type )
}

/* ---------------------------------------------------------------- */
/* Resources ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Get contents of resource file like ":/myText.html"
// as a QString.
//
void res2Str( QString &str, const QString resFile )
{
    QFile   f( resFile );

    if( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
        str = QTextStream( &f ).readAll();
    else
        str.clear();
}


QString rmvLastSlash( const QString &path )
{
    QString _path = path;
    QRegExp re("[/\\\\]+$");
    int     i = _path.indexOf( re );

    if( i > 0 )
        _path.truncate( i );

    return _path;
}


QString appPath()
{
    QString path = QDir::currentPath();

#ifdef Q_OS_MAC
    QDir    D( path );

    if( D.cdUp() )
        path = D.canonicalPath();
#endif

    return path;
}


QString configPath( const QString &fileName )
{
    return QString("%1/_Configs/%2.ini")
            .arg( appPath() )
            .arg( fileName );
}


QString calibPath()
{
    return QString("%1/_Calibration")
            .arg( appPath() );
}


QString calibPath( const QString &fileName )
{
    return QString("%1/_Calibration/%2.ini")
            .arg( appPath() )
            .arg( fileName );
}


bool toolPath( QString &path, const QString &toolName, bool bcreate )
{
    path = QString("%1/_Tools").arg( appPath() );

    if( bcreate ) {

        if( !QDir().mkpath( path ) ) {
            Error() << "Failed to create folder [" << path << "].";
            return false;
        }
    }

    path = QString("%1/%2").arg( path ).arg( toolName );

    return true;
}

/* ---------------------------------------------------------------- */
/* Timers --------------------------------------------------------- */
/* ---------------------------------------------------------------- */

static QMutex   dtStrMutex;


QString dateTime2Str( const QDateTime &dt, Qt::DateFormat f )
{
    QMutexLocker    ml( &dtStrMutex );
    return dt.toString( f );
}


QString dateTime2Str( const QDateTime &dt, const QString &format )
{
    QMutexLocker    ml( &dtStrMutex );
    return dt.toString( format );
}

/* ---------------------------------------------------------------- */
/* end namespace Util --------------------------------------------- */
/* ---------------------------------------------------------------- */

}   // namespace Util


