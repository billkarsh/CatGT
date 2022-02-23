
#include "Pass1AP2LF.h"
#include "CGBL.h"
#include "Util.h"

#define MAX10BIT    512




/* ---------------------------------------------------------------- */
/* Dwn1IO --------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// All EOF tracking done at 30 kHz. Only thing different
// is this function will only write every twelfth sample.
// Do that by sliding the samples forward in buf.
//
// Counting lessons:
//
// xxLxxxxxxxxxxxL... : Buffer holding nsmp
// xxL                : first (partial) group
//
// offset : zero based step to next L
//
// (A) offset + 1               : count spanned by first group
// (B) nsmp - (A)               : count excluding first group
// (C) (nsmp - offset - 1) / 12 : num whole groups after first
// (D) nlf = 1 + (C)            : num L in buffer (first + wholes)
// (E) (A) + 12 * (C)           : count spanned by L's
// (E) offset + 1 + 12 * (nlf - 1)
// (F) nsmp - (E)               : remainder count
// (G) 12 - (F)                 : count spanned by first group of next buffer
// (H) 11 - (F)                 : offset to first L of next buffer
// (H) 11 - nsmp + offset + 1 + 12 * (nlf - 1)
// (H) offset' = 12 - nsmp + offset + 12 * (nlf - 1)
//
// For zero filling, want remaining count of last buffer:
// (I) offset' + 1  : count spanned by first group of "next" virtual buffer
// (J) 12 - (I)     : remainder in last buffer
// (J) 11 - offset' : remainder from last file
//
qint64 Dwn1IO::_write( qint64 bytes )
{
    int nC          = meta.nC,
        smpBytes    = nC * sizeof(qint16),
        nsmp        = bytes / smpBytes;

    if( offset >= nsmp ) {
        offset -= nsmp;
        return bytes;
    }

    int nlf = 1 + (nsmp - offset - 1) / 12,
        nmv = nlf,
        d12 = 12 * nC,
        wrt;

    qint16  *dst = &o_buf[0],
            *src = &o_buf[offset * nC];

    if( !offset ) {
        dst += nC;
        src += d12;
        --nmv;
    }

    for( int i = 0; i < nmv; ++i, dst += nC, src += d12 )
        memcpy( dst, src, smpBytes );

    offset = 12 - nsmp + offset + 12 * (nlf - 1);

    wrt = nlf * smpBytes;

    if( wrt != o_f.write( o_buf8(), wrt ) )
        return 0;

    return bytes;
}


bool Dwn1IO::zero( qint64 gapBytes, qint64 zfBytes )
{
    gapBytes = meta.smpBytes * (11 - offset + gapBytes/meta.smpBytes) / 12;
    zfBytes  = meta.smpBytes * (11 - offset + zfBytes /meta.smpBytes) / 12;
    offset   = 0;   // want first sample of next file

    Log() <<
    QString("Gap before file '%1' out_start_smp=%2 inp_gap_smp=%3 out_zeros_smp=%4")
    .arg( i_fi.fileName() )
    .arg( meta.smpOutSpan() / 12 )
    .arg( gapBytes / meta.smpBytes )
    .arg( zfBytes / meta.smpBytes );

    if( zfBytes <= 0 )
        return true;

    qint64  o_bufBytes = o_buf.size() * sizeof(qint16);

    memset( o_buf8(), 0, o_bufBytes );

    do {

        qint64  cpyBytes = qMin( zfBytes, o_bufBytes );

        if( cpyBytes != Pass1IO::_write( cpyBytes ) ) {
            Log() << QString("Zero fill failed (error %1); input file '%2'.")
                        .arg( o_f.error() )
                        .arg( i_fi.fileName() );
            return false;
        }

        zfBytes        -= cpyBytes;
        meta.smpOutEOF += 12 * cpyBytes / meta.smpBytes;

    } while( zfBytes > 0 );

    return true;
}

/* ---------------------------------------------------------------- */
/* Pass1AP2LF ----------------------------------------------------- */
/* ---------------------------------------------------------------- */

bool Pass1AP2LF::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    io.doWrite = GBL.gt_nIndices() > 1 || GBL.lfflt.isenabled() || GBL.tshift;

    switch( openInputMeta( fim, meta.kvp, g0, t0, ip, 0, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return false;

    if( !io.o_open( g0, ip, 0, 1 ) )
        return false;

    meta.read( ip );

    filtersAndScaling();

    gFOff.init( meta.srate / 12, ip, 1 );

    io.alloc( true );

    io.run();

    adjustMeta();
    meta.write( io.o_name, g0, t0, ip, 1 );

    gFOff.dwnSmp( ip );

    return true;
}


void Pass1AP2LF::digital( const qint16 *data, int ntpts )
{
    Q_UNUSED( data )
    Q_UNUSED( ntpts )
}


void Pass1AP2LF::neural( qint16 *data, int ntpts )
{
    Q_UNUSED( data )
    Q_UNUSED( ntpts )
}


void Pass1AP2LF::filtersAndScaling()
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


// To turn 2.0 AP file into LF-like file:
// - filename .ap. -> .lf. (handled by meta.set())
// - firstSample n -> n/12.
// - fool smpOutEOF, hence, smpOutSpan() for call to meta.set()
// - imSampleRate n -> n/12.
// - snsChanMap rename AP -> LF.
//
void Pass1AP2LF::adjustMeta()
{
// firstSample

    meta.kvp["firstSample"] = meta.smp1st /= 12;

    meta.smpOutEOF = meta.smp1st + io.o_f.size() / meta.smpBytes;

// imSampleRate

    meta.kvp["imSampRate"] = meta.srate /= 12;

// snsChanMap

    meta.kvp["~snsChanMap"] =
        meta.kvp["~snsChanMap"].toString().replace( "AP", "LF" );
}


