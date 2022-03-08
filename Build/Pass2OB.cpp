
#include "Pass2OB.h"
#include "CGBL.h"
#include "Util.h"




bool Pass2OB::first( int ip )
{
    int g0 = GBL.gt_get_first( 0 );

    samps       = 0;
    this->ip    = ip;

    {
        QFileInfo   fim;

        if( openInputMeta( fim, meta.kvp, g0, -1, OB, ip, false ) )
            return false;
    }

    if( !GBL.sc_skipbin && !openOutputBinary( fout, outBin, g0, OB, ip ) )
        return false;

    meta.read( OB, ip );

    initDigitalFields();

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, OB, ip );

    return next( 0 );
}


bool Pass2OB::next( int ie )
{
    qint64  ieSamps = p2_checkCounts( meta, ie, OB, ip );

    if( !ieSamps )
        return false;

    if( !GBL.sc_skipbin && !p2_openAndCopyFile( fout, meta, buf, 0, ie, OB, ip, eBIN ) )
        return false;

    if( !copyDigitalFiles( ie ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, OB, ip );

    meta.pass2_runDone();

    return true;
}


void Pass2OB::close()
{
    if( closedIP != ip ) {
        int g0 = GBL.gt_get_first( 0 );
        fout.close();
        meta.smpOutEOF = meta.smp1st + samps;
        meta.write( outBin, g0, -1, OB, ip );
        closedIP = ip;
    }
}


void Pass2OB::initDigitalFields()
{
//@OBX TODO need universal extractors
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


bool Pass2OB::openDigitalFiles( int g0 )
{
//@OBX TODO need universal extractors
    for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

        TTLD    &T = GBL.SY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.obOutFile( g0, ip, eSY, &T ) ) )
            return false;
    }

    for( int i = 0, n = GBL.iSY.size(); i < n; ++i ) {

        TTLD    &T = GBL.iSY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.obOutFile( g0, ip, eiSY, &T ) ) )
            return false;
    }

    return true;
}


bool Pass2OB::copyDigitalFiles( int ie )
{
//@OBX TODO need universal extractors
    for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

        TTLD    &T = GBL.SY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, OB, ip, eSY, &T ) )
            return false;
    }

    for( int i = 0, n = GBL.iSY.size(); i < n; ++i ) {

        TTLD    &T = GBL.iSY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, OB, ip, eiSY, &T ) )
            return false;
    }

    return true;
}


