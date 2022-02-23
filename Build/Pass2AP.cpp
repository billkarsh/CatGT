
#include "Pass2AP.h"
#include "CGBL.h"
#include "Util.h"




// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail.
//
int Pass2AP::first( int ip )
{
    int g0 = GBL.gt_get_first( 0 );

    samps       = 0;
    this->ip    = ip;

    {
        QFileInfo   fim;
        int         ret = openInputMeta( fim, meta.kvp, g0, -1, ip, 0, GBL.prb_miss_ok );

        if( ret )
            return ret;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return 2;

    if( !openOutputBinary( fout, outBin, g0, ip ) )
        return 2;

    meta.read( ip );

    initDigitalFields();

    if( !openDigitalFiles( g0 ) )
        return 2;

    gFOff.init( meta.srate, ip );

    if( !next( 0 ) )
        return 2;

    return 0;
}


bool Pass2AP::next( int ie )
{
    qint64  ieSamps = p2_checkCounts( meta, ie, ip, 0 );

    if( !ieSamps )
        return false;

    if( !p2_openAndCopyFile( fout, meta, buf, 0, ie, ip, 0 ) )
        return false;

    if( !copyDigitalFiles( ie ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, ip );

    meta.pass2_runDone();

    return true;
}


void Pass2AP::close()
{
    if( closedIP != ip ) {
        int g0 = GBL.gt_get_first( 0 );
        fout.close();
        meta.smpOutEOF = meta.smp1st + samps;
        meta.write( outBin, g0, -1, ip );
        closedIP = ip;
    }
}


void Pass2AP::initDigitalFields()
{
    for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

        TTLD    &T = GBL.SY[i];

        if( T.ip == ip )
            T.autoWord( meta.nC );
    }

    for( int i = 0, n = GBL.iSY.size(); i < n; ++i ) {

        TTLD    &T = GBL.iSY[i];

        if( T.ip == ip )
            T.autoWord( meta.nC );
    }
}


bool Pass2AP::openDigitalFiles( int g0 )
{
    for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

        TTLD    &T = GBL.SY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.imOutFile( g0, ip, 2, 0, &T ) ) )
            return false;
    }

    for( int i = 0, n = GBL.iSY.size(); i < n; ++i ) {

        TTLD    &T = GBL.iSY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.imOutFile( g0, ip, 5, 0, &T ) ) )
            return false;
    }

    return true;
}


bool Pass2AP::copyDigitalFiles( int ie )
{
    for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

        TTLD    &T = GBL.SY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, ip, 2, 0, &T ) )
            return false;
    }

    for( int i = 0, n = GBL.iSY.size(); i < n; ++i ) {

        TTLD    &T = GBL.iSY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, ip, 5, 0, &T ) )
            return false;
    }

    return true;
}


