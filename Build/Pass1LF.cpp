
#include "Pass1LF.h"
#include "CGBL.h"
#include "Util.h"

#define MAX10BIT    512




bool Pass1LF::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    io.doWrite = GBL.gt_nIndices() > 1 || GBL.lfflt.isenabled() || GBL.tshift;

    switch( openInputMeta( fim, meta.kvp, g0, t0, ip, 1, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return false;

    if( !io.o_open( g0, ip, 1, 1 ) )
        return false;

    meta.read( ip, 1 );

    filtersAndScaling();

    io.alloc( true );

    io.run();

    meta.write( io.o_name, g0, t0, ip, 1 );

    return true;
}


void Pass1LF::digital( const qint16 *data, int ntpts )
{
    Q_UNUSED( data )
    Q_UNUSED( ntpts )
}


void Pass1LF::neural( qint16 *data, int ntpts )
{
    Q_UNUSED( data )
    Q_UNUSED( ntpts )
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


