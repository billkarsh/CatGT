
#include "Pass1NI.h"
#include "CGBL.h"
#include "Util.h"




bool Pass1NI::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    io.doWrite = GBL.force_ni_ob || GBL.gt_nIndices() > 1;

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
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word < meta.nC )
            X->scan( data, meta.smpInpSpan(), ntpts, meta.nC );
    }
}


void Pass1NI::initDigitalFields()
{
    ex0 = GBL.myXrange( exLim, NI, 0 );

    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        X->autoWord( meta.nC );

        if( X->word < meta.nC )
            X->init( meta.srate, meta.kvp["niAiRangeMax"].toDouble() );
    }
}


bool Pass1NI::openDigitalFiles( int g0 )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= meta.nC )
            continue;

        if( !X->openOutFiles( g0, NI, 0 ) )
            return false;
    }

    return true;
}


