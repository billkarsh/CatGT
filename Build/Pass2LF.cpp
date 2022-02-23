
#include "Pass2LF.h"
#include "CGBL.h"
#include "Util.h"




// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail.
//
int Pass2LF::first( int ip )
{
    int g0 = GBL.gt_get_first( 0 );

    samps       = 0;
    this->ip    = ip;

    {
        QFileInfo   fim;
        int         ret = openInputMeta( fim, meta.kvp, g0, -1, ip, 1, GBL.prb_miss_ok );

        if( ret )
            return ret;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return 2;

    if( !openOutputBinary( fout, outBin, g0, ip, 1 ) )
        return 2;

    meta.read( ip, 1 );

    gFOff.init( meta.srate, ip, 1 );

    if( !next( 0 ) )
        return 2;

    return 0;
}


bool Pass2LF::next( int ie )
{
    qint64  ieSamps = p2_checkCounts( meta, ie, ip, 1 );

    if( !ieSamps )
        return false;

    if( !p2_openAndCopyFile( fout, meta, buf, 0, ie, ip, 0, 1 ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, ip, 1 );

    meta.pass2_runDone();

    return true;
}


void Pass2LF::close()
{
    if( closedIP != ip ) {
        int g0 = GBL.gt_get_first( 0 );
        fout.close();
        meta.smpOutEOF = meta.smp1st + samps;
        meta.write( outBin, g0, -1, ip, 1 );
        closedIP = ip;
    }
}


