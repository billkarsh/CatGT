
#include "Pass1LF.h"
#include "Util.h"

#define MAX10BIT    512




bool Pass1LF::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 ),
        theZ;

    switch( GBL.openInputMeta( fim, meta.kvp, g0, t0, LF, ip, ip, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    if( !parseMaxZ( theZ ) )
        return false;

    mySrange();

    doWrite = GBL.gt_nIndices() > 1
                || svLim > sv0 || GBL.startsecs >= 0
                || GBL.lfflt.isenabled() || GBL.tshift;

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return false;

    if( !o_open( g0 ) )
        return false;

    meta.read( fim, LF, ip );

    for( int is = sv0; is < svLim; ++is ) {
        if( !GBL.vS[is].init( meta.kvp, fim, theZ ) )
            return false;
    }

    if( !filtersAndScaling() )
        return false;

    gFOff.init( meta.srate, LF, ip );

    alloc();

    fileLoop();

    if( GBL.startsecs >= 0 )
        meta.kvp["firstSample"] = meta.smp1st;

    if( svLim > sv0 )
        meta.writeSave( sv0, svLim, g0, t0, LF );
    else
        meta.write( o_name, g0, t0, LF, ip, ip );

    return true;
}


bool Pass1LF::filtersAndScaling()
{
    IMROTbl *R = GBL.getProbe( meta.kvp );

    if( R ) {
        maxInt = R->maxInt();
        delete R;
    }
    else {
        Log() << QString("Can't identify probe type in metadata '%1'.")
                    .arg( fim.fileName() );
        return false;
    }

    return true;
}


