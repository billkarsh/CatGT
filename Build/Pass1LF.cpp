
#include "Pass1LF.h"
#include "Util.h"

#define MAX10BIT    512




bool Pass1LF::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    io.doWrite = GBL.gt_nIndices() > 1 || GBL.lfflt.isenabled() || GBL.tshift;

    switch( openInputMeta( fim, meta.kvp, g0, t0, LF, ip, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return false;

    if( !io.o_open( g0, LF, LF, ip ) )
        return false;

    meta.read( LF, ip );

    filtersAndScaling();

    gFOff.init( meta.srate, LF, ip );

    io.alloc();

    io.run();

    meta.write( io.o_name, g0, t0, LF, ip );

    return true;
}


void Pass1LF::filtersAndScaling()
{
// ----
// Imro
// ----

// 3A default

    int maxInt = MAX10BIT;

// Get actual

    IMROTbl *R = getProbe( meta.kvp );

    if( R ) {
        maxInt = R->maxInt();
        delete R;
    }

    io.set_maxInt( maxInt );
}


