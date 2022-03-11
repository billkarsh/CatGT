
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
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word < meta.nC )
            X->scan( data, meta.smpInpSpan(), ntpts, meta.nC );
    }
}


void Pass1OB::initDigitalFields()
{
    ex0 = GBL.myXrange( exLim, OB, ip );

    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        X->autoWord( meta.nC );

        if( X->word < meta.nC )
            X->init( meta.srate, meta.kvp["obAiRangeMax"].toDouble() );
    }
}


bool Pass1OB::openDigitalFiles( int g0 )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= meta.nC )
            continue;

        if( !X->openOutFiles( g0, OB, ip ) )
            return false;
    }

    return true;
}


