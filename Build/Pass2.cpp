
#include "Pass2.h"
#include "Util.h"




Pass2::Pass2( std::vector<BTYPE> &buf, t_js js )
    :   buf(buf), closedIP(-9), js(js), miss_ok(false), do_bin(true)
{
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
int Pass2::first( int ip )
{
    int g0 = GBL.gt_get_first( 0 );

    samps       = 0;
    this->ip    = ip;

    {
        QFileInfo   fim;
        int         ret = openInputMeta( fim, meta.kvp, g0, -1, js, ip, miss_ok );

        if( ret )
            return ret;
    }

    if( js >= AP && !GBL.makeOutputProbeFolder( g0, ip ) )
        return 2;

    if( do_bin && !openOutputBinary( fout, outBin, g0, js, ip ) )
        return 2;

    meta.read( js, ip );

    if( js != LF ) {

        initDigitalFields();

        if( !openDigitalFiles( g0 ) )
            return 2;
    }

    gFOff.init( meta.srate, js, ip );

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

    if( js != LF && !copyDigitalFiles( ie ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, js, ip );

    meta.pass2_runDone();

    return true;
}


void Pass2::close()
{
    if( closedIP != ip ) {
        int g0 = GBL.gt_get_first( 0 );
        fout.close();
        meta.smpOutEOF = meta.smp1st + samps;
        meta.write( outBin, g0, -1, js, ip );
        closedIP = ip;
    }
}


void Pass2::initDigitalFields()
{
    ex0 = GBL.myXrange( exLim, js, ip );

    for( int i = ex0; i < exLim; ++i )
        GBL.vX[i]->autoWord( meta.nC );
}


bool Pass2::openDigitalFiles( int g0 )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= meta.nC )
            continue;

        if( !X->openOutFiles( g0, js, ip ) )
            return false;
    }

    return true;
}


bool Pass2::copyDigitalFiles( int ie )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= meta.nC )
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
    QFileInfo   fim;
    KVParams    kvpn;
    int         chans;

    if( openInputMeta( fim, kvpn, GBL.ga, -1, js, ip, false ) )
        return 0;

    chans = kvpn["nSavedChans"].toInt();

    if( chans != meta.nC ) {
        Log() <<
        QString("Error at supercat run '%1': Channel count [%2]"
        " does not match run-zero count [%3].")
        .arg( ie ).arg( chans ).arg( meta.nC );
        return 0;
    }

    if( GBL.sc_trim ) {
        Elem    &E = GBL.velem[ie];
        return  qint64(E.tail( js, ip ) * meta.srate + 1)
                - qint64(E.head( js, ip ) * meta.srate)
                - (ie ? 1 : 0);
    }

    return kvpn["fileSizeBytes"].toLongLong() / (sizeof(qint16) * meta.nC);
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
    int                 ip )
{
    qint64  head     = GBL.velem[ie].head( js, ip ) * meta.srate,
            tail     = GBL.velem[ie].tail( js, ip ) * meta.srate + 1,
            bufBytes = sizeof(BTYPE) * buf.size(),
            finBytes = fin.size();

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

        finBytes       -= cpyBytes;
        meta.smpOutEOF += cpySamps;
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

        finBytes       -= cpyBytes;
        meta.smpOutEOF += cpySamps;
    }

    return true;
}


// Trimming version.
//
static void copyOffsetTimes(
    QFile   &fin,
    double  secs,
    int     ie,
    t_js    js,
    int     ip,
    XTR     *X )
{
    double  t,
            head = GBL.velem[ie].head( js, ip ),
            tail = GBL.velem[ie].tail( js, ip );
    QString line;
    bool    ok;

    if( ie )
        secs -= head;

    while( !(line = fin.readLine()).isEmpty() ) {

        t = line.toDouble( &ok );

        if( ok ) {
            if( t > head && t <= tail )
                *X->ts << QString("%1\n").arg( t + secs, 0, 'f', 6 );
        }
        else
            *X->ts << line;
    }
}


static void copyOffsetTimes( QFile &fin, double secs, XTR *X )
{
    double  t;
    QString line;
    bool    ok;

    while( !(line = fin.readLine()).isEmpty() ) {

        t = line.toDouble( &ok );

        if( ok )
            *X->ts << QString("%1\n").arg( t + secs, 0, 'f', 6 );
        else
            *X->ts << line;
    }
}


bool Pass2::copyFile( QFile &fout, int ie, t_ex ex, XTR *X )
{
    QFile       fin;
    QFileInfo   fib;

    if( openInputFile( fin, fib, GBL.ga, -1, js, ip, ex, X ) )
        return false;

    if( !X ) {

        if( GBL.sc_trim ) {

            if( !copyBinary( fout, fin, fib, meta, buf, ie, js, ip ) )
                return false;
        }
        else if( !copyBinary( fout, fin, fib, meta, buf ) )
            return false;
    }
    else if( GBL.sc_trim )
        copyOffsetTimes( fin, samps / meta.srate, ie, js, ip, X );
    else
        copyOffsetTimes( fin, samps / meta.srate, X );

    return true;
}


bool Pass2::copyFilesBF( int ie, BitField *B )
{
    QFile       ftin, fvin;
    QFileInfo   ftib, fvib;

    if( openInputFile( ftin, ftib, GBL.ga, -1, js, ip, eBFT, B ) )
        return false;

    if( openInputFile( fvin, fvib, GBL.ga, -1, js, ip, eBFV, B ) )
        return false;

    if( GBL.sc_trim ) { // tandem trim and copy

        double  t,
                secs = samps / meta.srate,
                head = GBL.velem[ie].head( js, ip ),
                tail = GBL.velem[ie].tail( js, ip );
        QString LT, LV;
        bool    ok;

        if( ie )
            secs -= head;

        while( !(LT = ftin.readLine()).isEmpty() ) {

            LV = fvin.readLine();
            t  = LT.toDouble( &ok );

            if( ok ) {
                if( t > head && t <= tail ) {
                    *B->ts  << QString("%1\n").arg( t + secs, 0, 'f', 6 );
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

        // offet times
        copyOffsetTimes( ftin, samps / meta.srate, B );

        // append unmodified values
        QString line;
        while( !(line = fvin.readLine()).isEmpty() )
            *B->tsv << line;
    }

    return true;
}


