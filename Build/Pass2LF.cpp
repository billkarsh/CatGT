
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
        int         ret = openInputMeta( fim, meta.kvp, g0, -1, LF, ip, GBL.prb_miss_ok );

        if( ret )
            return ret;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return 2;

    if( !openOutputBinary( fout, outBin, g0, LF, ip ) )
        return 2;

    meta.read( LF, ip );

    gFOff.init( meta.srate, LF, ip );

    if( !next( 0 ) )
        return 2;

    return 0;
}


bool Pass2LF::next( int ie )
{
    qint64  ieSamps = p2_checkCounts( meta, ie, LF, ip );

    if( !ieSamps )
        return false;

    if( !p2_openAndCopyFile( fout, meta, buf, 0, ie, LF, ip, eBIN ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, LF, ip );

    meta.pass2_runDone();

    return true;
}


void Pass2LF::close()
{
    if( closedIP != ip ) {
        int g0 = GBL.gt_get_first( 0 );
        fout.close();
        meta.smpOutEOF = meta.smp1st + samps;
        meta.write( outBin, g0, -1, LF, ip );
        closedIP = ip;
    }
}


