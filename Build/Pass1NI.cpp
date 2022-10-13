
#include "Pass1NI.h"
#include "Util.h"




bool Pass1NI::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    doWrite = GBL.force_ni_ob || GBL.gt_nIndices() > 1 || GBL.startsecs > 0;

    if( GBL.openInputMeta( fim, meta.kvp, g0, t0, NI, 0, false ) )
        return false;

    if( !o_open( g0 ) )
        return false;

    meta.read( NI, 0 );

    initDigitalFields( meta.kvp["niAiRangeMax"].toDouble() );

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, NI, 0 );

    alloc();

    fileLoop();

    meta.write( o_name, g0, t0, NI, 0 );

    return true;
}


