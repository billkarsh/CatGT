
#include "Pass2.h"
#include "Util.h"

QMap<int,int>   Pass2::ip1rep;
QMutex          Pass2::ip1Mtx;


Pass2::Pass2( t_js js, int ip2 )
    :   js(js), ip2(ip2), miss_ok(false), do_bin(true)
{
    buf.resize( 32 * 1024 * 1024 / sizeof(BTYPE) );

    if( js >= AP )
        miss_ok = GBL.prb_miss_ok;
    else
        do_bin = !GBL.sc_skipbin;
}


// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail.
//
int Pass2::first()
{
    int g0 = GBL.velem[0].g;

    samps   = 0;
    ip1     = (js >= AP ? GBL.velem[0].mip2ip1[ip2] : ip2);

    set_ip1rep();

    QFileInfo   fim;
    int         ret = GBL.openInputMeta( fim, meta.kvp, 0, g0, -1, js, ip1, ip2, miss_ok );
    if( ret )
        return ret;

    if( is_ip1rep() && !GBL.makeOutputProbeFolder( g0, ip1 ) )
        return 2;

    if( do_bin && !GBL.openOutputBinary( fout, outBin, 0, g0, js, ip1, ip2 ) )
        return 2;

    meta.read( fim, js, -1 );

    if( js != LF && is_ip1rep() ) {

        initDigitalFields();

        if( !openDigitalFiles( g0 ) )
            return 2;
    }

    gFOff.init( meta.srate, js, ip2 );

    if( !next( 0 ) )
        return 2;

    return 0;
}


bool Pass2::next( int ie )
{
    qint64  ieSamps = checkCounts( ie );

    if( !ieSamps )
        return false;

    if( do_bin && !copyFile( fout, ie, eBIN ) )
        return false;

    if( js != LF && is_ip1rep() && !copyDigitalFiles( ie ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, js, ip2 );

    meta.pass2_runDone();

    return true;
}


void Pass2::close()
{
    fout.close();

    if( meta.nC ) {
        meta.smpWritten = samps;
        meta.write( outBin, 0, -1, -1, js, ip1, ip2 );
    }
}


void Pass2::set_ip1rep()
{
    if( js >= AP ) {
        QMutexLocker    ml( &ip1Mtx );
        if( !ip1rep.contains( ip1 ) )
            ip1rep[ip1] = ip2;
    }
}


bool Pass2::is_ip1rep()
{
    if( js >= AP ) {
        QMutexLocker    ml( &ip1Mtx );
        QMap<int,int>::const_iterator
            it  = ip1rep.find( ip1 ),
            end = ip1rep.end();
        if( it == end || it.value() != ip2 )
            return false;
    }

    return true;
}


void Pass2::initDigitalFields()
{
    ex0 = GBL.myXrange( exLim, js, ip1 );

    for( int i = ex0; i < exLim; ++i )
        GBL.vX[i]->autoWord( GBL.velem[0].nC( js, ip1 ) );
}


bool Pass2::openDigitalFiles( int g0 )
{
    QVector<Save>   vSdum;

    for( int i = ex0; i < exLim; ++i ) {

        XTR *X  = GBL.vX[i];
        int nC  = GBL.velem[0].nC( js, ip1 );

        if( X->word >= nC ) {
            X->wordError( nC );
            continue;
        }

        if( !X->openOutFiles( vSdum, 0, g0 ) )
            return false;
    }

    return true;
}


bool Pass2::copyDigitalFiles( int ie )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= GBL.velem[0].nC( js, ip1 ) )
            continue;

        bool    ok;

        if( X->ex == eBFT )
            ok = copyFilesBF( ie, reinterpret_cast<BitField*>(X) );
        else
            ok = copyFile( *X->f, ie, X->ex, X );

        if( !ok )
            return false;
    }

    return true;
}


// - Check channel count matches first run.
// - Return sample count, or zero if error.
//
qint64 Pass2::checkCounts( int ie )
{
    Elem        &E = GBL.velem[ie];
    QFileInfo   fim;
    KVParams    kvpn;
    int         chans;

    if( GBL.openInputMeta( fim, kvpn, ie, E.g, -1, js, ip1, ip2, false ) )
        return 0;

    chans = kvpn["nSavedChans"].toInt();

    if( chans != meta.nC ) {
        Log() <<
        QString("Error at supercat run '%1': Channel count [%2]"
        " does not match run-zero count [%3], file: '%4'.")
        .arg( ie ).arg( chans ).arg( meta.nC ).arg( fim.fileName() ) ;
        return 0;
    }

    double  rate = E.rate( js, ip1 );

    gFOff.addRate( rate, js, ip2 );

    if( GBL.sc_trim ) {
        // Basic count includes head and tail, so (tail - head + 1).
        // But each edge should be included just once, so if later
        // than the first file (ie > 0), we don't include the head,
        // so subtract 1 for that: - (ie ? 1 : 0).
        return  qint64(E.tail( js, ip1 ) * rate + 1)
                - qint64(E.head( js, ip1 ) * rate)
                - (ie ? 1 : 0);
    }

    return kvpn["fileSizeBytes"].toLongLong() / (sizeof(qint16) * chans);
}


// Trimming version.
//
static bool copyBinary(
    QFile               &fout,
    QFile               &fin,
    const QFileInfo     &fib,
    Meta                &meta,
    std::vector<BTYPE>  &buf,
    int                 ie,
    t_js                js,
    int                 ip1 )
{
    Elem    &E          = GBL.velem[ie];
    double  rate        = E.rate( js, ip1 );
    qint64  head        = E.head( js, ip1 ) * rate,
            tail        = E.tail( js, ip1 ) * rate + 1,
            bufBytes    = sizeof(BTYPE) * buf.size(),
            finBytes    = fin.size();

    if( finBytes % meta.smpBytes ) {

        finBytes = (finBytes / meta.smpBytes) * meta.smpBytes;

        Log() <<
        QString("Warning: Untrimmed binary file not a multiple of sample size '%1'.")
        .arg( fib.filePath() );
    }

    if( ie ) {
        ++head;
        fin.seek( head * meta.smpBytes );
    }

    finBytes = qMin( finBytes, (tail - head) * meta.smpBytes );

    while( finBytes > 0 ) {

        int cpySamps,
            cpyBytes = qMin( bufBytes, finBytes );

        cpySamps = cpyBytes / meta.smpBytes;
        cpyBytes = cpySamps * meta.smpBytes;

        if( cpyBytes != fin.read( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Read failed for file '%1'.").arg( fib.filePath() );
            return false;
        }

        if( cpyBytes != fout.write( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Write failed (error %1) for file '%2'.")
                        .arg( fout.error() )
                        .arg( fib.filePath() );
            return false;
        }

        finBytes        -= cpyBytes;
        meta.smpWritten += cpySamps;
    }

    return true;
}


static bool copyBinary(
    QFile               &fout,
    QFile               &fin,
    const QFileInfo     &fib,
    Meta                &meta,
    std::vector<BTYPE>  &buf )
{
    qint64  bufBytes = sizeof(BTYPE) * buf.size(),
            finBytes = fin.size();

    if( finBytes % meta.smpBytes ) {
        Log() << QString("Binary file not a multiple of sample size '%1'.").arg( fib.filePath() );
        return false;
    }

    while( finBytes > 0 ) {

        int cpySamps,
            cpyBytes = qMin( bufBytes, finBytes );

        cpySamps = cpyBytes / meta.smpBytes;
        cpyBytes = cpySamps * meta.smpBytes;

        if( cpyBytes != fin.read( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Read failed for file '%1'.").arg( fib.filePath() );
            return false;
        }

        if( cpyBytes != fout.write( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Write failed (error %1) for file '%2'.")
                        .arg( fout.error() )
                        .arg( fib.filePath() );
            return false;
        }

        finBytes        -= cpyBytes;
        meta.smpWritten += cpySamps;
    }

    return true;
}


// Trimming version.
//
static void copyOffsetTimesTrim(
    QFile   &fin,
    qint64  samps,
    int     ie,
    t_js    js,
    int     ip,
    XTR     *X )
{
    Elem    &E      = GBL.velem[ie];
    double  t,
            rate0   = GBL.velem[0].rate( js, ip ),
            secs    = samps / rate0,
            conv    = E.rate( js, ip ) / rate0,
            head    = E.head( js, ip ),
            tail    = E.tail( js, ip );
    QString line;
    bool    ok;

    if( ie ) {
        // trim up to and including head sample
        secs -= (head * conv) + 1.0 / rate0;
    }

    while( !(line = fin.readLine()).isEmpty() ) {

        t = line.toDouble( &ok );

        if( ok ) {
            if( t > head && t <= tail )
                *X->ts << QString("%1\n").arg( t * conv + secs, 0, 'f', 6 );
        }
        else
            *X->ts << line;
    }
}


static void copyOffsetTimes(
    QFile   &fin,
    qint64  samps,
    int     ie,
    t_js    js,
    int     ip1,
    XTR     *X )
{
    double  t,
            rate0 = GBL.velem[0].rate( js, ip1 ),
            secs  = samps / rate0,
            conv  = GBL.velem[ie].rate( js, ip1 ) / rate0;
    QString line;
    bool    ok;

    while( !(line = fin.readLine()).isEmpty() ) {

        t = line.toDouble( &ok );

        if( ok )
            *X->ts << QString("%1\n").arg( t * conv + secs, 0, 'f', 6 );
        else
            *X->ts << line;
    }
}


bool Pass2::copyFile( QFile &fout, int ie, t_ex ex, XTR *X )
{
    QFile       fin;
    QFileInfo   fib;

    if( GBL.openInputFile( fin, fib, ie, -1, -1, js, ip1, (X ? ip1 : ip2), ex, X ) )
        return false;

    if( !X ) {

        if( !meta.smpBytes )
            return true;

        if( GBL.sc_trim ) {

            if( !copyBinary( fout, fin, fib, meta, buf, ie, js, ip1 ) )
                return false;
        }
        else if( !copyBinary( fout, fin, fib, meta, buf ) )
            return false;
    }
    else if( GBL.sc_trim )
        copyOffsetTimesTrim( fin, samps, ie, js, ip1, X );
    else
        copyOffsetTimes( fin, samps, ie, js, ip1, X );

    return true;
}


bool Pass2::copyFilesBF( int ie, BitField *B )
{
    Elem        &E = GBL.velem[ie];
    QFile       ftin, fvin;
    QFileInfo   ftib, fvib;

    if( GBL.openInputFile( ftin, ftib, ie, E.g, -1, js, ip1, ip1, eBFT, B ) )
        return false;

    if( GBL.openInputFile( fvin, fvib, ie, E.g, -1, js, ip1, ip1, eBFV, B ) )
        return false;

    if( GBL.sc_trim ) { // tandem trim and copy

        double  t,
                rate0 = GBL.velem[0].rate( js, ip1 ),
                secs  = samps / rate0,
                conv  = E.rate( js, ip1 ) / rate0,
                head  = E.head( js, ip1 ),
                tail  = E.tail( js, ip1 );
        QString LT, LV;
        bool    ok;

        if( ie )
            secs -= head * conv;

        while( !(LT = ftin.readLine()).isEmpty() ) {

            LV = fvin.readLine();
            t  = LT.toDouble( &ok );

            if( ok ) {
                if( t > head && t <= tail ) {
                    *B->ts  << QString("%1\n").arg( t * conv + secs, 0, 'f', 6 );
                    *B->tsv << LV;
                }
            }
            else {
                *B->ts  << LT;
                *B->tsv << LV;
            }
        }
    }
    else {
        // offset times
        copyOffsetTimes( ftin, samps, ie, js, ip1, B );

        // append unmodified values
        QString line;
        while( !(line = fvin.readLine()).isEmpty() )
            *B->tsv << line;
    }

    return true;
}


