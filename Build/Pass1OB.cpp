
#include "Pass1OB.h"
#include "Util.h"




bool Pass1OB::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    doWrite = GBL.force_ni_ob || GBL.gt_nIndices() > 1 || GBL.startsecs > 0;

    if( GBL.openInputMeta( fim, meta.kvp, g0, t0, OB, ip, false ) )
        return false;

    if( !o_open( g0 ) )
        return false;

    meta.read( OB );

    initDigitalFields( meta.kvp["obAiRangeMax"].toDouble() );

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, OB, ip );

    alloc();

    fileLoop();

    meta.write( o_name, g0, t0, OB, ip, ip );

    return true;
}


