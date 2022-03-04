
#include "Pass1NI.h"
#include "CGBL.h"
#include "Util.h"




bool Pass1NI::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    io.doWrite = GBL.force_ni || GBL.gt_nIndices() > 1;

    if( openInputMeta( fim, meta.kvp, g0, t0, NI, 0, false ) )
        return false;

    if( !io.o_open( g0, NI, NI, 0 ) )
        return false;

    meta.read( NI, 0 );

    initDigitalFields();

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, NI, 0 );

    io.alloc();

    io.run();

    meta.write( io.o_name, g0, t0, NI, 0 );

    return true;
}


void Pass1NI::digital( const qint16 *data, int ntpts )
{
    for( int i = 0, n = GBL.XA.size(); i < n; ++i ) {

        TTLA    &T = GBL.XA[i];

        if( T.word >= meta.nC )
            continue;

        T.XA( data, meta.smpInpSpan(), ntpts, meta.nC );
    }

    for( int i = 0, n = GBL.XD.size(); i < n; ++i ) {

        TTLD    &T = GBL.XD[i];

        if( T.word >= meta.nC )
            continue;

        T.XD( data, meta.smpInpSpan(), ntpts, meta.nC );
    }

    for( int i = 0, n = GBL.iXA.size(); i < n; ++i ) {

        TTLA    &T = GBL.iXA[i];

        if( T.word >= meta.nC )
            continue;

        T.iXA( data, meta.smpInpSpan(), ntpts, meta.nC );
    }

    for( int i = 0, n = GBL.iXD.size(); i < n; ++i ) {

        TTLD    &T = GBL.iXD[i];

        if( T.word >= meta.nC )
            continue;

        T.iXD( data, meta.smpInpSpan(), ntpts, meta.nC );
    }

    for( int i = 0, n = GBL.BF.size(); i < n; ++i ) {

        XBF &B = GBL.BF[i];

        if( B.word >= meta.nC )
            continue;

        B.BF( data, meta.smpInpSpan(), ntpts, meta.nC );
    }
}


void Pass1NI::neural( qint16 *data, int ntpts )
{
    Q_UNUSED( data )
    Q_UNUSED( ntpts )
}


void Pass1NI::initDigitalFields()
{
    for( int i = 0, n = GBL.XA.size(); i < n; ++i ) {

        TTLA    &T = GBL.XA[i];

        if( T.word >= meta.nC )
            continue;

        T.setTolerance( meta.srate );

        // assume gain = 1

        double  rangeMax = meta.kvp["niAiRangeMax"].toDouble();
        T.T = SHRT_MAX * T.thresh / rangeMax;
        T.V = SHRT_MAX * T.thrsh2 / rangeMax;
    }

    for( int i = 0, n = GBL.XD.size(); i < n; ++i ) {

        TTLD    &T = GBL.XD[i];

        T.autoWord( meta.nC );

        if( T.word < meta.nC )
            T.setTolerance( meta.srate );
    }

    for( int i = 0, n = GBL.iXA.size(); i < n; ++i ) {

        TTLA    &T = GBL.iXA[i];

        if( T.word >= meta.nC )
            continue;

        T.setTolerance( meta.srate );

        // assume gain = 1

        double  rangeMax = meta.kvp["niAiRangeMax"].toDouble();
        T.T = SHRT_MAX * T.thresh / rangeMax;
        T.V = SHRT_MAX * T.thrsh2 / rangeMax;
    }

    for( int i = 0, n = GBL.iXD.size(); i < n; ++i ) {

        TTLD    &T = GBL.iXD[i];

        T.autoWord( meta.nC );

        if( T.word < meta.nC )
            T.setTolerance( meta.srate );
    }

    for( int i = 0, n = GBL.BF.size(); i < n; ++i ) {

        XBF &B = GBL.BF[i];

        B.autoWord( meta.nC );

        if( B.word < meta.nC )
            B.initMask( meta.srate );
    }
}


bool Pass1NI::openDigitalFiles( int g0 )
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


