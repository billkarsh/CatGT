
#include "Pass1OB.h"
#include "CGBL.h"
#include "Util.h"




bool Pass1OB::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    io.doWrite = GBL.force_ni_ob || GBL.gt_nIndices() > 1;

    if( openInputMeta( fim, meta.kvp, g0, t0, OB, ip, false ) )
        return false;

    if( !io.o_open( g0, OB, OB, ip ) )
        return false;

    meta.read( OB, ip );

    initDigitalFields();

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, OB, ip );

    io.alloc( true );

    io.run();

    meta.write( io.o_name, g0, t0, OB, ip );

    return true;
}


void Pass1OB::digital( const qint16 *data, int ntpts )
{
//@OBX TODO need universal extractors
    for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

        TTLD    &T = GBL.SY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        T.XD( data, meta.smpInpSpan(), ntpts, meta.nC );
    }

    for( int i = 0, n = GBL.iSY.size(); i < n; ++i ) {

        TTLD    &T = GBL.iSY[i];

        if( T.ip != ip || T.word >= meta.nC )
            continue;

        T.iXD( data, meta.smpInpSpan(), ntpts, meta.nC );
    }
}


void Pass1OB::initDigitalFields()
{
//@OBX TODO need universal extractors
    for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

        TTLD    &T = GBL.SY[i];

        if( T.ip != ip )
            continue;

        T.autoWord( meta.nC );

        if( T.word < meta.nC )
            T.setTolerance( meta.srate );
    }

    for( int i = 0, n = GBL.iSY.size(); i < n; ++i ) {

        TTLD    &T = GBL.iSY[i];

        if( T.ip != ip )
            continue;

        T.autoWord( meta.nC );

        if( T.word < meta.nC )
            T.setTolerance( meta.srate );
    }
}


bool Pass1OB::openDigitalFiles( int g0 )
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


