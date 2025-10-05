
#include "Pass1AP2LF.h"
#include "Util.h"

#define MAX10BIT    512




bool Pass1AP2LF::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 ),
        theZ;

    switch( GBL.openInputMeta( fim, meta.kvp, g0, t0, AP, ip, ip, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    if( !splitShanks() )
        return false;

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

    meta.read( fim, AP, ip );

    for( int is = sv0; is < svLim; ++is ) {
        if( !GBL.vS[is].init( meta.kvp, fim, theZ ) )
            return false;
    }

    if( !filtersAndScaling() )
        return false;

    GBL.mjsiprate[JSIP(LF,ip)] = meta.srate / GBL.ap2lf_dwnsmp;
    gFOff.init( meta.srate / GBL.ap2lf_dwnsmp, LF, ip );

    alloc();

    fileLoop();

    adjustMeta();

    if( svLim > sv0 )
        meta.writeSave( sv0, svLim, g0, t0, LF );
    else
        meta.write( o_name, g0, t0, LF, ip, ip );

    gFOff.dwnSmp( ip );

    return true;
}


// All EOF tracking done at 30 kHz. Only thing different is
// this function will only write every ap2lf_dwnsmp (DS) sample.
// Do that by sliding the samples forward in buf.
//
// Counting lessons: (DS=12 in figure)
//
// xxLxxxxxxxxxxxL... : Buffer holding nsmp
// xxL                : first (partial) group
//
// offset : zero based step to next L
//
// (A) offset + 1               : count spanned by first group
// (B) nsmp - (A)               : count excluding first group
// (C) (nsmp - offset - 1) / DS : num whole groups after first
// (D) nlf = 1 + (C)            : num L in buffer (first + wholes)
// (E) (A) + DS * (C)           : count spanned by L's
// (E) offset + 1 + DS * (nlf - 1)
// (F) nsmp - (E)               : remainder count
// (G) DS - (F)                 : count spanned by first group of next buffer
// (H) DS - 1 - (F)             : offset to first L of next buffer
// (H) DS - 1 - nsmp + offset + 1 + DS * (nlf - 1)
// (H) offset' = DS - nsmp + offset + DS * (nlf - 1)
//
// For zero filling, want remaining count of last buffer:
// (I) offset' + 1      : count spanned by first group of "next" virtual buffer
// (J) DS - (I)         : remainder in last buffer
// (J) DS - 1 - offset' : remainder from last file
//
bool Pass1AP2LF::_write( qint64 bytes )
{
    int nC          = meta.nC,
        smpBytes    = nC * sizeof(qint16),
        nsmp        = bytes / smpBytes,
        DS          = GBL.ap2lf_dwnsmp;

    if( offset >= nsmp ) {
        offset -= nsmp;
        return true;
    }

    int nlf = 1 + (nsmp - offset - 1) / DS,
        nmv = nlf,
        dDS = DS * nC,
        wrt;

    qint16  *dst = &o_buf[0],
            *src = &o_buf[offset * nC];

    if( !offset ) {
        dst += nC;
        src += dDS;
        --nmv;
    }

    for( int i = 0; i < nmv; ++i, dst += nC, src += dDS )
        memcpy( dst, src, smpBytes );

    offset = DS - nsmp + offset + DS * (nlf - 1);

    wrt = nlf * smpBytes;

    return Pass1::_write( wrt );
}


bool Pass1AP2LF::zero( qint64 gapBytes, qint64 zfBytes )
{
    int DS = GBL.ap2lf_dwnsmp;

    gapBytes = meta.smpBytes * (DS - 1 - offset + gapBytes/meta.smpBytes) / DS;
    zfBytes  = meta.smpBytes * (DS - 1 - offset + zfBytes /meta.smpBytes) / DS;
    offset   = 0;   // want first sample of next file

    Log() <<
    QString("Gap before file '%1' out_start_smp=%2 inp_gap_smp=%3 out_zeros_smp=%4")
    .arg( i_fi.fileName() )
    .arg( meta.smpWritten / DS )
    .arg( gapBytes / meta.smpBytes )
    .arg( zfBytes / meta.smpBytes );

    if( zfBytes <= 0 )
        return true;

    if( js_in >= AP && GBL.linefil ) {
        vSeg.push_back(
        LineSeg( meta.smpWritten / DS, zfBytes / meta.smpBytes ) );
    }

    qint64  o_bufBytes = o_buf.size() * sizeof(qint16);

    memset( o_buf8(), 0, o_bufBytes );

    do {

        qint64  cpyBytes = qMin( zfBytes, o_bufBytes );

        if( !Pass1::_write( cpyBytes ) )
            return false;

        zfBytes         -= cpyBytes;
        meta.smpWritten += DS * cpyBytes / meta.smpBytes;

    } while( zfBytes > 0 );

    return true;
}


bool Pass1AP2LF::filtersAndScaling()
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


// To turn AP file into LF-like file:
// - filename .ap. -> .lf. (handled by meta.set())
// - firstSample n -> n/DS.
// - smpWritten measured.
// - imSampleRate n -> n/DS.
// - snsChanMap rename AP -> LF.
//
void Pass1AP2LF::adjustMeta()
{
// firstSample

    meta.kvp["firstSample"] = meta.smp1st /= GBL.ap2lf_dwnsmp;

    if( svLim > sv0 ) {
        // EOF as samples same for all vS[]
        const Save &S = GBL.vS[sv0];
        meta.smpWritten = S.o_f->size() / S.smpBytes;
    }
    else
        meta.smpWritten = o_f.size() / meta.smpBytes;

// imSampleRate

    meta.kvp["imSampRate"] = meta.srate /= GBL.ap2lf_dwnsmp;

// snsChanMap

    meta.kvp["~snsChanMap"] =
        meta.kvp["~snsChanMap"].toString().replace( "AP", "LF" );
}


