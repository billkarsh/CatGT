
#include "Pass1.h"
#include "Util.h"
#include "Biquad.h"


// With FFT-based filtering one must use a long enough FFT to adequately
// sample low frequencies. A one second span is good down to 1 Hz (really,
// 0.5 Hz not too bad), so handles LFP band data without much distortion.
// 32768 is a little longer than one second.
//
// Freq domain filtering will cause some ringing in low freq components
// so we process the data in overlapped 32768 sample chunks, throwing
// away about 500 points from each chunk end (except at the file ends).
//
// - FFT is used for {tshift, filters}.
// - FFT chunks are a power of 2 in size.
// - FFT ops make edge artifacts so we trim margins and keep middles.
// - FFT chunks are, therefore, overlapped.
// - The middles are resulting OBUFs.
// - The input buffer holds NMID output middles to reduce reload frequency.
// - The input buffer holds a premargin for FFT overlap and gfix look-back.
// - The input buffer holds a postmargin for FFT overlap and gfix look-ahead.

#define SZFFT   32768
#define SZMRG   500
#define SZMID   (SZFFT - 2*SZMRG)
#define SZOBUF  SZFFT
#define NMID    1
#define SZIBUF  (NMID*SZMID + 2*SZMRG)

#define IBUF( i_pos )   &i_buf[(i_pos)*meta.nC]
#define IBYT( i_pos )   ((i_pos)*meta.smpBytes)


/* ---------------------------------------------------------------- */
/* protected ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

Pass1::~Pass1()
{
    if( hipass ) delete hipass;
    if( lopass ) delete lopass;
}


bool Pass1::o_open( int g0 )
{
    if( !doWrite )
        return true;

    return openOutputBinary( o_f, o_name, g0, js_out, ip );
}


void Pass1::initDigitalFields( double rngMax )
{
    ex0 = GBL.myXrange( exLim, js_in, ip );

    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        X->autoWord( meta.nC );

        if( X->word < meta.nC )
            X->init( meta.srate, rngMax );
    }
}


bool Pass1::openDigitalFiles( int g0 )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= meta.nC )
            continue;

        if( !X->openOutFiles( g0, js_in, ip ) )
            return false;
    }

    return true;
}


// Must be called after Meta::get().
//
void Pass1::alloc()
{
    i_buf.resize( SZIBUF * meta.nC );
    o_buf.resize( SZOBUF * meta.nC );

    if( js_in >= AP ) {

        if( js_out == AP && GBL.apflt.isbiquad() ) {
            if( GBL.apflt.Fhi )
                hipass = new Biquad( bq_type_highpass, GBL.apflt.Fhi / meta.srate );
            if( GBL.apflt.Flo )
                lopass = new Biquad( bq_type_lowpass, GBL.apflt.Flo / meta.srate );
        }
        else if( js_out == LF && GBL.lfflt.isbiquad() ) {
            if( GBL.lfflt.Fhi )
                hipass = new Biquad( bq_type_highpass, GBL.lfflt.Fhi / meta.srate );
            if( GBL.lfflt.Flo )
                lopass = new Biquad( bq_type_lowpass, GBL.lfflt.Flo / meta.srate );
        }

        fft.init( fim, meta, js_in, js_out );
    }
}


// Driving loop over files moving input to output.
//
void Pass1::fileLoop()
{
    GT_iterator I;
    int         g, t;

    while( I.next( g, t ) ) {

        // -----------
        // Open binary
        // -----------

        switch( openInputBinary( i_f, i_fi, g, t, js_in, ip ) ) {
            case 0: break;
            case 1: continue;
            case 2: return;
        }

        // ------------------
        // File size, overlap
        // ------------------

        qint64  xferBytes;

        switch( inputSizeAndOverlap( xferBytes, g, t ) ) {

            case 0: break;
            case 1: continue;
            case 2: return;
        }

        // ----------------------------
        // Process and copy binary data
        // ----------------------------

        while( xferBytes ) {

            if( !load( xferBytes ) || !push() ) {
                i_f.close();
                return;
            }
        }

        i_f.close();
        meta.pass1_fileDone( g, t, js_out, ip );

        if( meta.smpInpEOF >= meta.maxOutEOF ) {
            flush();
            return;
        }
    }

    flush();
}


void Pass1::digital( const qint16 *data, int ntpts )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word < meta.nC )
            X->scan( data, meta.smpInpSpan(), ntpts, meta.nC );
    }
}


qint64 Pass1::_write( qint64 bytes )
{
    return o_f.write( o_buf8(), bytes );
}


bool Pass1::zero( qint64 gapBytes, qint64 zfBytes )
{
    Log() <<
    QString("Gap before file '%1' out_start_smp=%2 inp_gap_smp=%3 out_zeros_smp=%4.")
    .arg( i_fi.fileName() )
    .arg( meta.smpOutSpan() )
    .arg( gapBytes / meta.smpBytes )
    .arg( zfBytes / meta.smpBytes );

    if( zfBytes <= 0 )
        return true;

    qint64  o_bufBytes = o_buf.size() * sizeof(qint16);

    memset( o_buf8(), 0, o_bufBytes );

    do {

        qint64  cpyBytes = qMin( zfBytes, o_bufBytes );

        if( cpyBytes != _write( cpyBytes ) ) {
            Log() << QString("Zero fill failed (error %1); input file '%2'.")
                        .arg( o_f.error() )
                        .arg( i_fi.fileName() );
            return false;
        }

        zfBytes        -= cpyBytes;
        meta.smpOutEOF += cpyBytes / meta.smpBytes;

    } while( zfBytes > 0 );

    return true;
}

/* ---------------------------------------------------------------- */
/* private -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// The next binary file was just opened...now...
// A lot going on here:
//
// xferBytes:
// How many bytes of the next binary to process AFTER this
// function completes. This size is not bounded by maxOutEOF.
//
// If the next file overlaps the previous one, we seek into
// the binary and reduce xferBytes. Processing of chunks can
// resume with xferBytes now added to any previous remainder.
//
// If there is a gap to the next file we check for remainder
// input data, and if present, process and write that. Then
// we optionally zero fill the output and update EOF trackers.
// Note EOF tracking is updated even if (doWrite) is false.
//
// Note on EOF tracking:
// In general we should (process and) write out everything we
// read in. smpInpEOF should be updated as we read. smpOutEOF
// should be updated when we write. Generally we will read in
// multiple chunks and some margin, then pass that to output
// one chunk at a time. Therefore, smpOutEOF will usually lag
// smpInpEOF. It is smpInpEOF that is bounded by maxOutEOF.
// Likewise, zero-filling (virtual input) should be bounded
// by maxOutEOF.
//
// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail or done.
//
int Pass1::inputSizeAndOverlap( qint64 &xferBytes, int g, int t )
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    if( g > g0 || t > t0 ) {

        // Not the first file, so get its meta data...

        QFileInfo   fim;
        KVParams    kvp;
        int         ret = openInputMeta( fim, kvp, g, t, js_in, ip, GBL.t_miss_ok );

        if( ret )
            return ret;

        xferBytes = kvp["fileSizeBytes"].toLongLong();

        qint64  olapSmp     = meta.smpInpEOF
                                - kvp["firstSample"].toLongLong(),
                olapBytes   = IBYT(olapSmp),
                fOffset;

        if( olapSmp > 0 ) {

            xferBytes -= olapBytes;

            if( xferBytes < meta.smpBytes ) {
                Log() <<
                    QString("Skipping tiny content"
                    " (olap: %1, rem: %2, bps: %3) file '%4'.")
                    .arg( olapBytes )
                    .arg( xferBytes )
                    .arg( meta.smpBytes )
                    .arg( i_fi.fileName() );
                i_f.close();
                return 1;
            }

            if( !i_f.seek( olapBytes ) ) {
                Log() << QString("Seek failed (offset: %1) for file '%2'.")
                            .arg( olapBytes )
                            .arg( i_fi.fileName() );
                i_f.close();
                return 2;
            }

            fOffset = meta.smpOutEOF + rem() - olapSmp;
        }
        else if( olapSmp < 0 ) {

            // Push any remainder data out.
            //
            // Note that this does not change the gap size used
            // for zeroFill because that is based on smpInpEOF.

            if( !flush() )
                return 2;

            if( !meta.pass1_zeroFill( *this, -olapBytes ) )
                return 2;

            fOffset = meta.smpOutEOF;
        }
        else
            fOffset = meta.smpOutEOF + rem();

        gFOff.addOffset( fOffset - meta.smp1st, js_out, ip );
    }
    else    // Already have meta data for first file
        xferBytes = meta.kvp["fileSizeBytes"].toLongLong();

    return 0;
}


// - Zero-pad LHS as needed.
// - Slide remainder forward.
// - Load from file.
// - Biquad loaded data in place.
// - Call digital() on loaded data.
//
bool Pass1::load( qint64 &xferBytes )
{
// Zero-pad and slide

    if( !i_nxt || i_nxt >= i_lim ) {
        // start fresh
        memset( &i_buf[0], 0, IBYT(SZMRG) );
        i_lim = SZMRG;
    }
    else {

        i_lim -= i_nxt - SZMRG;

        if( i_nxt > SZMRG )
            memcpy( &i_buf[0], IBUF(i_nxt - SZMRG), IBYT(i_lim) );
    }

// Load

    qint64  bytes;
    int     ntpts;

    bytes = meta.pass1_sizeRead( ntpts, xferBytes, IBYT(SZIBUF - i_lim) );

    if( bytes != i_f.read( (char*)IBUF(i_lim), bytes ) ) {
        Log() << QString("Read failed for file '%1'.").arg( i_fi.fileName() );
        return false;
    }

// Biquad

    if( hipass )
        hipass->applyBlockwiseMem( IBUF(i_lim), maxInt, ntpts, meta.nC, 0, meta.nN );

    if( lopass )
        lopass->applyBlockwiseMem( IBUF(i_lim), maxInt, ntpts, meta.nC, 0, meta.nN );

// Digital

    digital( IBUF(i_lim), ntpts );

// Indices

    i_nxt  = SZMRG;
    i_lim += ntpts;

    xferBytes       -= bytes;
    meta.smpInpEOF  += ntpts;

    return true;
}


// Process and write neural middles.
//
bool Pass1::push()
{
    while( i_nxt + SZMID + SZMRG <= i_lim ) {

        fft.apply( &o_buf[0], IBUF(i_nxt - SZMRG),
                SZMID, SZFFT,
                meta.nC, meta.nN );

        gfix0 = i_nxt;

        neural( &o_buf[0], SZMID );

        i_nxt += SZMID;

        if( !write( IBYT(SZMID) ) )
            return false;
    }

    return true;
}


int Pass1::rem()
{
    if( !i_nxt || i_nxt >= i_lim )
        return 0;

    return i_lim - i_nxt;
}


// Process and write any neural remainder.
//
bool Pass1::flush()
{
    int ndst = rem(),
        src0 = i_nxt - SZMRG;

    if( ndst <= 0 )
        return true;

    fft.apply( &o_buf[0], IBUF(src0),
            ndst, i_lim - src0,
            meta.nC, meta.nN );

    gfix0 = i_nxt;

    neural( &o_buf[0], ndst );

    i_nxt = 0;
    i_lim = 0;

    return write( IBYT(ndst) );
}


bool Pass1::write( qint64 bytes )
{
    if( doWrite && bytes != _write( bytes ) ) {
        Log() << QString("Write failed (error %1); input file '%2'.")
                    .arg( o_f.error() )
                    .arg( i_fi.fileName() );
        return false;
    }

    meta.smpOutEOF += bytes / meta.smpBytes;

    return true;
}


