
#include "Pass2NI.h"
#include "CGBL.h"
#include "Util.h"




bool Pass2NI::first()
{
    int g0 = GBL.gt_get_first( 0 );

    samps = 0;

    {
        QFileInfo   fim;

        if( openInputMeta( fim, meta.kvp, g0, -1, NI, 0, false ) )
            return false;
    }

    if( !GBL.sc_skipbin && !openOutputBinary( fout, outBin, g0, NI, 0 ) )
        return false;

    meta.read( NI, 0 );

    initDigitalFields();

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, NI, 0 );

    return next( 0 );
}


bool Pass2NI::next( int ie )
{
    qint64  ieSamps = p2_checkCounts( meta, ie, NI, 0 );

    if( !ieSamps )
        return false;

    if( !GBL.sc_skipbin && !p2_openAndCopyFile( fout, meta, buf, 0, ie, NI, 0, eBIN ) )
        return false;

    if( !copyDigitalFiles( ie ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, NI, 0 );

    meta.pass2_runDone();

    return true;
}


void Pass2NI::close()
{
    if( !closed ) {
        int g0 = GBL.gt_get_first( 0 );
        fout.close();
        meta.smpOutEOF = meta.smp1st + samps;
        meta.write( outBin, g0, -1, NI, 0 );
        closed = true;
    }
}


void Pass2NI::initDigitalFields()
{
    for( int i = 0, n = GBL.XD.size(); i < n; ++i )
        GBL.XD[i].autoWord( meta.nC );

    for( int i = 0, n = GBL.iXD.size(); i < n; ++i )
        GBL.iXD[i].autoWord( meta.nC );

    for( int i = 0, n = GBL.BF.size(); i < n; ++i )
        GBL.BF[i].autoWord( meta.nC );
}


bool Pass2NI::openDigitalFiles( int g0 )
{
    for( int i = 0, n = GBL.XA.size(); i < n; ++i ) {

        TTLA    &T = GBL.XA[i];

        if( T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.niOutFile( g0, eXA, &T ) ) )
            return false;
    }

    for( int i = 0, n = GBL.XD.size(); i < n; ++i ) {

        TTLD    &T = GBL.XD[i];

        if( T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.niOutFile( g0, eXD, &T ) ) )
            return false;
    }

    for( int i = 0, n = GBL.iXA.size(); i < n; ++i ) {

        TTLA    &T = GBL.iXA[i];

        if( T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.niOutFile( g0, eiXA, &T ) ) )
            return false;
    }

    for( int i = 0, n = GBL.iXD.size(); i < n; ++i ) {

        TTLD    &T = GBL.iXD[i];

        if( T.word >= meta.nC )
            continue;

        if( !T.openOutTimesFile( GBL.niOutFile( g0, eiXD, &T ) ) )
            return false;
    }

    for( int i = 0, n = GBL.BF.size(); i < n; ++i ) {

        XBF &B = GBL.BF[i];

        if( B.word >= meta.nC )
            continue;

        if( !B.openOutTimesFile( GBL.niOutFile( g0, eBFT, &B ) ) )
            return false;

        if( !B.openOutValsFile( GBL.niOutFile( g0, eBFV, &B ) ) )
            return false;
    }

    return true;
}


bool Pass2NI::copyDigitalFiles( int ie )
{
    for( int i = 0, n = GBL.XA.size(); i < n; ++i ) {

        TTLA    &T = GBL.XA[i];

        if( T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, NI, 0, eXA, &T ) )
            return false;
    }

    for( int i = 0, n = GBL.XD.size(); i < n; ++i ) {

        TTLD    &T = GBL.XD[i];

        if( T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, NI, 0, eXD, &T ) )
            return false;
    }

    for( int i = 0, n = GBL.iXA.size(); i < n; ++i ) {

        TTLA    &T = GBL.iXA[i];

        if( T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, NI, 0, eiXA, &T ) )
            return false;
    }

    for( int i = 0, n = GBL.iXD.size(); i < n; ++i ) {

        TTLD    &T = GBL.iXD[i];

        if( T.word >= meta.nC )
            continue;

        if( !p2_openAndCopyFile( *T.f, meta, buf, samps, ie, NI, 0, eiXD, &T ) )
            return false;
    }

    for( int i = 0, n = GBL.BF.size(); i < n; ++i ) {

        XBF &B = GBL.BF[i];

        if( B.word >= meta.nC )
            continue;

        if( !p2_openAndCopyBFFiles( meta, samps, ie, NI, 0, B ) )
            return false;
    }

    return true;
}


