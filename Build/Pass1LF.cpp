
#include "Pass1LF.h"
#include "Util.h"

#define MAX10BIT    512




bool Pass1LF::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    doWrite = GBL.gt_nIndices() > 1
                || GBL.startsecs > 0 || GBL.lfflt.isenabled() || GBL.tshift;

    switch( GBL.openInputMeta( fim, meta.kvp, g0, t0, LF, ip, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return false;

    if( !o_open( g0 ) )
        return false;

    meta.read( LF, ip );

    filtersAndScaling();

    gFOff.init( meta.srate, LF, ip );

    alloc();

    fileLoop();

    meta.write( o_name, g0, t0, LF, ip );

    return true;
}


void Pass1LF::filtersAndScaling()
{
// ----
// Imro
// ----

// 3A default

    maxInt = MAX10BIT;

// Get actual

    IMROTbl *R = GBL.getProbe( meta.kvp );

    if( R ) {
        maxInt = R->maxInt();
        delete R;
    }
}


