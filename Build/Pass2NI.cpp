
#include "Pass2NI.h"
#include "CGBL.h"
#include "Util.h"




bool Pass2NI::first()
{
    int g0 = GBL.gt_get_first( 0 );

    samps = 0;

    {
        QFileInfo   fim;

        if( openInputMeta( fim, meta.kvp, g0, -1, NI, 0, false ) )
            return false;
    }

    if( !GBL.sc_skipbin && !openOutputBinary( fout, outBin, g0, NI, 0 ) )
        return false;

    meta.read( NI, 0 );

    initDigitalFields();

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, NI, 0 );

    return next( 0 );
}


bool Pass2NI::next( int ie )
{
    qint64  ieSamps = p2_checkCounts( meta, ie, NI, 0 );

    if( !ieSamps )
        return false;

    if( !GBL.sc_skipbin && !p2_openAndCopyFile( fout, meta, buf, 0, ie, NI, 0, eBIN ) )
        return false;

    if( !copyDigitalFiles( ie ) )
        return false;

    samps += ieSamps;

    if( ie < GBL.velem.size() - 1 )
        gFOff.addOffset( samps, NI, 0 );

    meta.pass2_runDone();

    return true;
}


void Pass2NI::close()
{
    if( !closed ) {
        int g0 = GBL.gt_get_first( 0 );
        fout.close();
        meta.smpOutEOF = meta.smp1st + samps;
        meta.write( outBin, g0, -1, NI, 0 );
        closed = true;
    }
}


void Pass2NI::initDigitalFields()
{
    ex0 = GBL.myXrange( exLim, NI, 0 );

    for( int i = ex0; i < exLim; ++i )
        GBL.vX[i]->autoWord( meta.nC );
}


bool Pass2NI::openDigitalFiles( int g0 )
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


bool Pass2NI::copyDigitalFiles( int ie )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= meta.nC )
            continue;

        bool    ok;

        if( X->ex == eBFT ) {
            ok = p2_openAndCopyBFFiles(
                    meta, samps, ie, NI, 0,
                    reinterpret_cast<BitField*>(X) );
        }
        else {
            ok = p2_openAndCopyFile(
                    *X->f, meta, buf, samps, ie, NI, 0, X->ex, X );
        }

        if( !ok )
            return false;
    }

    return true;
}


