
#include "CGBL.h"
#include "Cmdline.h"
#include "Util.h"
#include "Subset.h"

#include <QDir>
#include <QSet>

#define DIGTOL  0.20


/* --------------------------------------------------------------- */
/* Globals ------------------------------------------------------- */
/* --------------------------------------------------------------- */

CGBL    GBL;

/* --------------------------------------------------------------- */
/* GT_iterator --------------------------------------------------- */
/* --------------------------------------------------------------- */

GT_iterator::GT_iterator()
{
    n    = GBL.gtlist.size();
    icur = -1;
}


bool GT_iterator::next( int &g, int &t )
{
    if( n ) {
        if( icur == -1 ) {
            icur = 0;
next_icur:
            ecur = GBL.gtlist[icur];
            g = ecur.g;
            t = ecur.ta;
        }
        else if( ++ecur.ta <= ecur.tb ) {
            g = ecur.g;
            t = ecur.ta;
        }
        else if( ++icur < n )
            goto next_icur;
        else
            return false;
    }
    else {
        if( icur == -1 ) {
            g = ecur.g  = GBL.ga;
            t = ecur.ta = GBL.ta;
            icur = 0;
        }
        else if( ++ecur.ta <= GBL.tb ) {
            g = ecur.g;
            t = ecur.ta;
        }
        else if( ++ecur.g <= GBL.gb ) {
            g = ecur.g;
            t = ecur.ta = GBL.ta;
        }
        else
            return false;
    }

    return true;
}

/* --------------------------------------------------------------- */
/* Filter -------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool Filter::parse( const QString &s )
{
    const QStringList   sl = s.split(
                                QRegExp(","),
                                QString::SkipEmptyParts );
    if( sl.size() != 4 ) {
        Log() << "Filters need 4 arguments: -xxfilter=type,order,Fhi,Flo";
        return false;
    }

    type = sl.at(0);

    if( type != "biquad" && type != "butter" ) {
        Log() << "Only biquad and butter filters are supported.";
        return false;
    }

    order   = sl.at(1).toInt();
    Fhi     = sl.at(2).toDouble();
    Flo     = sl.at(3).toDouble();

    if( isbiquad() )
        order = 2;

    return true;
}

/* --------------------------------------------------------------- */
/* TTL ----------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool XCT::openOutTimesFile( const QString &file )
{
    f = new QFile( file );

    if( !f->open( QIODevice::WriteOnly | QIODevice::Text ) ) {
        Log() << QString("Error opening '%1'.").arg( file );
        return false;
    }

    f->resize( 0 );
    ts = new QTextStream( f );
    return true;
}


void XCT::close()
{
    if( ts )
        ts->flush();

    if( f )
        f->close();
}


QString TTL::sSpan()
{
    QString s = QString("%1").arg( span );
    s.replace( ".", "p" );
    return s;
}


void TTL::setTolerance( double rate )
{
    srate = rate;

    if( tol >= 0 ) {
        spanlo = (span - tol) * 1e-3 * rate;
        spanhi = (span + tol) * 1e-3 * rate;
    }
    else {
        spanlo = span * (1.0 - DIGTOL) * 1e-3 * rate;
        spanhi = span * (1.0 + DIGTOL) * 1e-3 * rate;
    }
}


QString TTLA::sparam( const QString &stype )
{
    QString s = QString(" -%1=%2,%3,%4,%5").arg( stype )
                    .arg( word ).arg( thresh ).arg( thrsh2 ).arg( span );

    if( tol >= 0 )
        s += QString(",%1").arg( tol );

    return s;
}


QString TTLA::suffix( const QString &stype )
{
    return QString(".%1_%2_%3.txt")
            .arg( stype ).arg( word ).arg( sSpan() );
}


void TTLA::XA( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// -------------------
// Must start on a low
// -------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( *d < T ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( *d >= T ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {

                    nrun = 0;

                    if( V <= T ) {
                        seek = 2;
                        goto seek_edge2;
                    }

                    seek = 3;
                    peak = 0;
                    goto seek_edge2_pk;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d >= T ) {

                        if( ++nrun >= GBL.inarow ) {

                            nrun = 0;

                            if( V <= T ) {
                                seek = 2;
                                goto seek_edge2;
                            }

                            seek = 3;
                            peak = 0;
                            goto seek_edge2_pk;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( *d < T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d < T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------------------------
// Seek edge2 candidate + thrsh2 check
// -----------------------------------

    if( seek == 3 ) {

seek_edge2_pk:
        while( (d += nC) < dlim ) {

            if( *d > peak )
                peak = *d;

            if( *d < T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d < T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    if( seek == 3 && peak < V ) {
        seek = 0;
        goto seek_init;
    }

    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


void TTLA::iXA( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// --------------------
// Must start on a high
// --------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( *d >= T ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( *d < T ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {

                    nrun = 0;

                    if( V >= T ) {
                        seek = 2;
                        goto seek_edge2;
                    }

                    seek = 3;
                    peak = 0;
                    goto seek_edge2_pk;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d < T ) {

                        if( ++nrun >= GBL.inarow ) {

                            nrun = 0;

                            if( V >= T ) {
                                seek = 2;
                                goto seek_edge2;
                            }

                            seek = 3;
                            peak = 0;
                            goto seek_edge2_pk;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( *d >= T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d >= T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------------------------
// Seek edge2 candidate + thrsh2 check
// -----------------------------------

    if( seek == 3 ) {

seek_edge2_pk:
        while( (d += nC) < dlim ) {

            if( *d < peak )
                peak = *d;

            if( *d >= T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d >= T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    if( seek == 3 && peak > V ) {
        seek = 0;
        goto seek_init;
    }

    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


QString TTLD::sparam( const QString &stype )
{
    QString s;

    if( stype.contains( "SY" ) ) {
        s = QString(" -%1=%2,%3,%4,%5").arg( stype )
                .arg( ip )
                .arg( word ).arg( bit ).arg( span );
    }
    else {
        s = QString(" -%1=%2,%3,%4").arg( stype )
                .arg( word ).arg( bit ).arg( span );
    }

    if( tol >= 0 )
        s += QString(",%1").arg( tol );

    return s;
}


QString TTLD::suffix( const QString &stype )
{
    return QString(".%1_%2_%3_%4.txt")
            .arg( stype ).arg( word ).arg( bit ).arg( sSpan() );
}


void TTLD::XD( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// -------------------
// Must start on a low
// -------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( !((*d >> bit) & 1) ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( (*d >> bit) & 1 ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {
                    seek = 2;
                    nrun = 0;
                    goto seek_edge2;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( (*d >> bit) & 1 ) {

                        if( ++nrun >= GBL.inarow ) {
                            seek = 2;
                            nrun = 0;
                            goto seek_edge2;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( !((*d >> bit) & 1) ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( !((*d >> bit) & 1) ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


void TTLD::iXD( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// --------------------
// Must start on a high
// --------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( (*d >> bit) & 1 ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( !((*d >> bit) & 1) ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {
                    seek = 2;
                    nrun = 0;
                    goto seek_edge2;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( !((*d >> bit) & 1) ) {

                        if( ++nrun >= GBL.inarow ) {
                            seek = 2;
                            nrun = 0;
                            goto seek_edge2;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( (*d >> bit) & 1 ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( (*d >> bit) & 1 ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


void XBF::initMask( double rate )
{
    srate = rate;

// mask = 2^nb - 1

    mask = 1;
    for( int i = 0; i < nb; ++i )
        mask *= 2;
    --mask;
}


QString XBF::sparam()
{
    return QString(" -BF=%1,%2,%3,%4")
            .arg( word ).arg( b0 ).arg( nb ).arg( inarow );
}


QString XBF::suffix( const QString &stype )
{
    return QString(".%1_%2_%3_%4.txt")
            .arg( stype ).arg( word ).arg( b0 ).arg( nb );
}


void XBF::BF( const qint16 *data, qint64 t0, int ntpts, int nC )
{
    const short *d = &data[word];

    for( int it = 0; it < ntpts; ++it, d += nC ) {

        uint v = (uint(*d) >> b0) & mask;

        if( v != vlast ) {

            if( inarow == 1 ) {
                // report now
                *ts  << QString("%1\n").arg( (t0 + it) / srate, 0, 'f', 6 );
                *tsv << QString("%1\n").arg( v );
                vlast = v;
            }
            else {
                // require inarow copies
                if( !nrun++ ) {
                    // new run
                    vrun = v;
                }
                else if( v != vrun ) {
                    // reset run count
                    nrun = 1;
                    vrun = v;
                }
                else if( nrun >= inarow ) {
                    // report start of run
                    *ts  << QString("%1\n").arg( (t0 + it + 1 - nrun) / srate, 0, 'f', 6 );
                    *tsv << QString("%1\n").arg( v );
                    vlast = v;
                    nrun  = 0;
                }
            }
        }
        else
            nrun = 0;
    }
}


bool XBF::openOutValsFile( const QString &file )
{
    fv = new QFile( file );

    if( !fv->open( QIODevice::WriteOnly | QIODevice::Text ) ) {
        Log() << QString("Error opening '%1'.").arg( file );
        return false;
    }

    fv->resize( 0 );
    tsv = new QTextStream( fv );
    return true;
}


void XBF::close()
{
    if( tsv )
        tsv->flush();

    if( fv )
        fv->close();

    XCT::close();
}

/* --------------------------------------------------------------- */
/* Elem ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

void Elem::unpack()
{
    GBL.inpar       = dir;
    GBL.run         = run;
    GBL.ga          = g;
    GBL.gb          = g;
    GBL.no_run_fld  = no_run_fld;
    GBL.catgt_fld   = catgt_fld;
}

/* --------------------------------------------------------------- */
/* PrintUsage  --------------------------------------------------- */
/* --------------------------------------------------------------- */

static void PrintUsage()
{
    Log();
    Log() << "*** ERROR: MISSING CRITICAL PARAMETERS ***\n";
    Log() << "------------------------";
    Log() << "Purpose:";
    Log() << "+ Optionally join trials with given run_name and index ranges [ga,gb] [ta,tb]...";
    Log() << "+ ...Or run on any individual file.";
    Log() << "+ Optionally apply demultiplexing corrections.";
    Log() << "+ Optionally apply band-pass and global CAR filters.";
    Log() << "+ Optionally edit out saturation artifacts.";
    Log() << "+ Optionally extract tables of sync waveform edge times to drive TPrime.";
    Log() << "+ Optionally extract tables of any other TTL event times to be aligned with spikes.";
    Log() << "+ Optionally join the above outputs across different runs (supercat feature).\n";
    Log() << "Output:";
    Log() << "+ Results are placed next to source, named like this, with t-index = tcat:";
    Log() << "+    path/run_name_g5_tcat.imec1.ap.bin.";
    Log() << "+ Errors and run messages are appended to CatGT.log in the current working directory.\n";
    Log() << "Usage:";
    Log() << ">CatGT -dir=data_dir -run=run_name -g=ga,gb -t=ta,tb <which streams> [ options ]\n";
    Log() << "Which streams:";
    Log() << "-ap                      ;required to process ap streams";
    Log() << "-lf                      ;required to process lf streams";
    Log() << "-ni                      ;required to process ni stream";
    Log() << "-prb_3A                  ;if -ap or -lf process 3A-style probe files, e.g. run_name_g0_t0.imec.ap.bin";
    Log() << "-prb=0,3:5               ;if -ap or -lf AND !prb_3A process these probes\n";
    Log() << "Options:";
    Log() << "-no_run_fld              ;older data, or data files relocated without a run folder";
    Log() << "-prb_fld                 ;use folder-per-probe organization";
    Log() << "-prb_miss_ok             ;instead of stopping, silently skip missing probes";
    Log() << "-gtlist={gj,tja,tjb}     ;override {-g,-t} giving each listed g-index its own t-range";
    Log() << "-t=cat                   ;extract TTL from CatGT output files (instead of -t=ta,tb)";
    Log() << "-exported                ;apply FileViewer 'exported' tag to in/output filenames";
    Log() << "-t_miss_ok               ;instead of stopping, zero-fill if trial missing";
    Log() << "-zerofillmax=500         ;set a maximum zero-fill span (millisec)";
    Log() << "-maxsecs=7.5             ;set a maximum output file length (float seconds)";
    Log() << "-apfilter=Typ,N,Fhi,Flo  ;apply ap band-pass filter of given {type, order, corners(float Hz)}";
    Log() << "-lffilter=Typ,N,Fhi,Flo  ;apply lf band-pass filter of given {type, order, corners(float Hz)}";
    Log() << "-no_tshift               ;DO NOT time-align channels to account for ADC multiplexing";
    Log() << "-loccar=2,8              ;apply ap local CAR annulus (exclude radius, include radius)";
    Log() << "-gblcar                  ;apply ap global CAR filter over all channels";
    Log() << "-gfix=0.40,0.10,0.02     ;rmv ap artifacts: ||amp(mV)||, ||slope(mV/sample)||, ||noise(mV)||";
    Log() << "-chnexcl={prb;chans}     ;this probe, exclude listed chans from ap loccar, gblcar, gfix";
    Log() << "-SY=0,384,6,10           ;extract TTL signal from imec SY (probe,word,bit,millisec)";
    Log() << "-XA=2,3.0,4.5,25         ;extract TTL signal from nidq XA (word,thresh1(v),thresh2(V),millisec)";
    Log() << "-XD=8,0,0                ;extract TTL signal from nidq XD (word,bit,millisec)";
    Log() << "-iSY=0,384,6,10          ;extract inverted TTL signal from imec SY (probe,word,bit,millisec)";
    Log() << "-iXA=2,2.0,1.0,25        ;extract inverted TTL signal from nidq XA (word,thresh1(v),thresh2(V),millisec)";
    Log() << "-iXD=8,0,0               ;extract inverted TTL signal from nidq XD (word,bit,millisec)";
    Log() << "-BF=8,2,4,3              ;extract numeric bit-field from nidq XD (word,startbit,nbits,inarow)";
    Log() << "-inarow=5                ;extractor antibounce stay high/low sample count";
    Log() << "-pass1_force_ni_ob_bin   ;write pass one ni/ob binary tcat file even if not changed";
    Log() << "-supercat={dir,run_ga}   ;concatenate existing output files across runs (see ReadMe)";
    Log() << "-supercat_trim_edges     ;supercat after trimming each stream to matched sync edges";
    Log() << "-supercat_skip_ni_ob_bin ;do not supercat ni/ob binary files";
    Log() << "-dest=path               ;alternate path for output files (must exist)";
    Log() << "-out_prb_fld             ;if using -dest, create output subfolder per probe";
    Log() << "------------------------\n";
}

/* ---------------------------------------------------------------- */
/* CGBL ----------------------------------------------------------- */
/* ---------------------------------------------------------------- */


bool CGBL::SetCmdLine( int argc, char* argv[] )
{
// Parse args

    QString     ssupercat;
    const char  *sarg = 0;

    for( int i = 1; i < argc; ++i ) {

        std::vector<double> vd;
        std::vector<int>    vi;

        if( GetArgStr( sarg, "-dir=", argv[i] ) )
            inpar = trim_adjust_slashes( sarg );
        else if( GetArgStr( sarg, "-run=", argv[i] ) )
            run = sarg;
        else if( GetArgStr( sarg, "-gtlist=", argv[i] ) ) {

            if( !gt_parse_list( sarg ) )
                return false;
        }
        else if( GetArgList( vi, "-g=", argv[i] ) ) {

            switch( vi.size() ) {
                case 2:
                    ga = vi[0];
                    gb = vi[1];
                    break;
                case 1:
                    gb = ga = vi[0];
                    break;
                default:
                    goto bad_param;
            }
        }
        else if( GetArgStr( sarg, "-t=", argv[i] ) ) {

            if( 0 == strcmp( sarg, "cat" ) )
                gt_set_tcat();
            else if( GetArgList( vi, "-t=", argv[i] ) ) {

                switch( vi.size() ) {
                    case 2:
                        ta = vi[0];
                        tb = vi[1];
                        break;
                    case 1:
                        tb = ta = vi[0];
                        break;
                    default:
                        goto bad_param;
                }
            }
            else
                goto bad_param;
        }
        else if( IsArg( "-no_run_fld", argv[i] ) )
            no_run_fld = true;
        else if( IsArg( "-prb_fld", argv[i] ) )
            prb_fld = true;
        else if( IsArg( "-prb_miss_ok", argv[i] ) )
            prb_miss_ok = true;
        else if( IsArg( "-exported", argv[i] ) )
            exported = true;
        else if( IsArg( "-t_miss_ok", argv[i] ) )
            t_miss_ok = true;
        else if( IsArg( "-ap", argv[i] ) )
            ap = true;
        else if( IsArg( "-lf", argv[i] ) )
            lf = true;
        else if( IsArg( "-ni", argv[i] ) )
            ni = true;
        else if( GetArgStr( sarg, "-prb=", argv[i] ) ) {

            if( !Subset::rngStr2Vec( vprb, sarg ) )
                goto bad_param;
        }
        else if( IsArg( "-prb_3A", argv[i] ) )
            prb_3A = true;
        else if( GetArg( &zfilmax, "-zerofillmax=%d", argv[i] ) )
            ;
        else if( GetArg( &maxsecs, "-maxsecs=%lf", argv[i] ) )
            ;
        else if( GetArgStr( sarg, "-apfilter=", argv[i] ) ) {
            if( !apflt.parse( sarg ) )
                return false;
        }
        else if( GetArgStr( sarg, "-lffilter=", argv[i] ) ) {
            if( !lfflt.parse( sarg ) )
                return false;
        }
        else if( GetArgList( vi, "-loccar=", argv[i] ) && vi.size() == 2 ) {

            locin  = vi[0];
            locout = vi[1];

            if( locin < 1 || locin >= locout ) {
                Log() <<
                    QString("Bad loccar parameters: -loccar=%1,%2.")
                    .arg( locin ).arg( locout );
                return false;
            }
        }
        else if( GetArg( &inarow, "-inarow=%d", argv[i] ) )
            ;
        else if( IsArg( "-no_tshift", argv[i] ) )
            tshift = false;
        else if( IsArg( "-gblcar", argv[i] ) )
            gblcar = true;
        else if( GetArgList( vd, "-gfix=", argv[i] ) && vd.size() == 3 ) {

            gfixamp = vd[0];
            gfixslp = vd[1];
            gfixbas = vd[2];
            gfixdo  = true;

            if( gfixamp < 0 )
                gfixamp = -gfixamp;

            if( gfixslp < 0 )
                gfixslp = -gfixslp;
        }
        else if( GetArgStr( sarg, "-chnexcl=", argv[i] ) ) {

            if( !parseChnexcl( sarg ) )
                return false;
        }
        else if( GetArgList( vd, "-SY=", argv[i] )
                && (vd.size() == 4 || vd.size() == 5) ) {

            TTLD    T;
            T.ip        = vd[0];
            T.word      = vd[1];
            T.bit       = vd[2];
            T.span      = vd[3];

            if( vd.size() == 5 )
                T.tol = vd[4];

            SY.push_back( T );
        }
        else if( GetArgList( vd, "-iSY=", argv[i] )
                && (vd.size() == 4 || vd.size() == 5) ) {

            TTLD    T;
            T.ip        = vd[0];
            T.word      = vd[1];
            T.bit       = vd[2];
            T.span      = vd[3];

            if( vd.size() == 5 )
                T.tol = vd[4];

            iSY.push_back( T );
        }
        else if( GetArgList( vd, "-XA=", argv[i] )
                && (vd.size() == 4 || vd.size() == 5) ) {

            TTLA    T;
            T.word      = vd[0];
            T.thresh    = vd[1];
            T.thrsh2    = vd[2];
            T.span      = vd[3];

            if( vd.size() == 5 )
                T.tol = vd[4];

            XA.push_back( T );
        }
        else if( GetArgList( vd, "-iXA=", argv[i] )
                && (vd.size() == 4 || vd.size() == 5) ) {

            TTLA    T;
            T.word      = vd[0];
            T.thresh    = vd[1];
            T.thrsh2    = vd[2];
            T.span      = vd[3];

            if( vd.size() == 5 )
                T.tol = vd[4];

            iXA.push_back( T );
        }
        else if( GetArgList( vd, "-XD=", argv[i] )
                && (vd.size() == 3 || vd.size() == 4) ) {

            TTLD    T;
            T.word      = vd[0];
            T.bit       = vd[1];
            T.span      = vd[2];

            if( vd.size() == 4 )
                T.tol = vd[3];

            XD.push_back( T );
        }
        else if( GetArgList( vd, "-iXD=", argv[i] )
                && (vd.size() == 3 || vd.size() == 4) ) {

            TTLD    T;
            T.word      = vd[0];
            T.bit       = vd[1];
            T.span      = vd[2];

            if( vd.size() == 4 )
                T.tol = vd[3];

            iXD.push_back( T );
        }
        else if( GetArgList( vd, "-BF=", argv[i] )
                && (vd.size() == 4) ) {

            XBF B;
            B.word      = vd[0];
            B.b0        = vd[1];
            B.nb        = vd[2];
            B.inarow    = vd[3];

            if( B.b0 < 0 || B.b0 > 15 || B.nb < 1 || B.nb > 16 - B.b0 ) {

                Log() <<
                "BF startbit must be in range [0..15], nbits in range [1..16-startbit].";
                return false;
            }

            if( B.inarow < 1 ) {
                B.inarow = 1;
                Log() << "Warning: BF inarow must be >= 1.";
            }

            BF.push_back( B );
        }
        else if( IsArg( "-pass1_force_ni_ob_bin", argv[i] ) )
            force_ni_ob = true;
        else if( GetArgStr( sarg, "-supercat=", argv[i] ) )
            ssupercat = sarg;
        else if( IsArg( "-supercat_trim_edges", argv[i] ) )
            sc_trim = true;
        else if( IsArg( "-supercat_skip_ni_ob_bin", argv[i] ) )
            sc_skipbin = true;
        else if( GetArgStr( sarg, "-dest=", argv[i] ) )
            opar = trim_adjust_slashes( sarg );
        else if( IsArg( "-out_prb_fld", argv[i] ) )
            out_prb_fld = true;
        else {
bad_param:
            Log() <<
            QString("Did not understand option '%1'.").arg( argv[i] );
            return false;
        }
    }

// Check args

    if( !ssupercat.isEmpty() && !parseElems( ssupercat ) )
        return false;

    if( velem.size() ) {

        if( opar.isEmpty() ) {
            Log() << "Error: Supercat requires -dest option.";
            goto error;
        }

        maxsecs     = 0;
        gfixamp     = 0;
        gfixslp     = 0;
        gfixbas     = 0;
        apflt.clear();
        lfflt.clear();
        mexc.clear();
        gtlist.clear();
        gt_set_tcat();
        zfilmax     = -1;
        locin       = 0;
        locout      = 0;
        inarow      = -1;
        t_miss_ok   = false;
        tshift      = false;
        gblcar      = false;
        gfixdo      = false;
    }
    else {

        if( inpar.isEmpty() ) {
            Log() << "Error: Missing -dir.";
            goto error;
        }

        if( run.isEmpty() ) {
            Log() << "Error: Missing -run.";
            goto error;
        }

        if( !gt_ok_indices() )
            goto error;
    }

    if( !ap && !lf && !ni ) {
        Log() << "Error: Missing stream indicator {-ap, -lf, -ni}.";
        goto error;
    }

    if( (ap || lf) && (!prb_3A && !vprb.size()) ) {

        Log() << "Error: Missing probe specifier {-prb_3A or -prb=???}.";
error:
        PrintUsage();
        return false;
    }

// Echo

    QString sreq        = "",
            sgt         = "",
            sprbs       = "",
            szfil       = "",
            smaxsecs    = "",
            sapfilter   = "",
            slffilter   = "",
            stshift     = "",
            sloccar     = "",
            sgfix       = "",
            schnexc     = "",
            sSY         = "",
            sXA         = "",
            sXD         = "",
            siSY        = "",
            siXA        = "",
            siXD        = "",
            sBF         = "",
            sinarow     = "",
            ssuper      = "",
            sodir       = "";

    if( !velem.size() ) {
        sreq    = QString(" -dir=%1 -run=%2").arg( inpar ).arg( run );
        sgt     = gt_format_params();
    }
    else
        sgt = " -t=cat";

    if( vprb.size() && !prb_3A )
        sprbs = " -prb=" + Subset::vec2RngStr( vprb );

    if( zfilmax >= 0 )
        szfil = QString(" -zerofillmax=%1").arg( zfilmax );

    if( maxsecs > 0 )
        smaxsecs = QString(" -maxsecs=%1").arg( maxsecs );

    if( apflt.isenabled() > 0 )
        sapfilter = QString(" -apfilter=%1").arg( apflt.format() );

    if( lfflt.isenabled() > 0 )
        slffilter = QString(" -lffilter=%1").arg( lfflt.format() );

    if( !velem.size() && !tshift )
        stshift = QString(" -no_tshift");

    if( locout > 0 )
        sloccar = QString(" -loccar=%1,%2").arg( locin ).arg( locout );

    if( gfixdo )
        sgfix = QString(" -gfix=%1,%2,%3").arg( gfixamp ).arg( gfixslp ).arg( gfixbas );

    if( mexc.size() )
        schnexc = QString(" -chnexcl=%1").arg( formatChnexcl() );

    foreach( TTLD T, SY )
        sSY += T.sparam( "SY" );

    foreach( TTLD T, iSY )
        siSY += T.sparam( "iSY" );

    foreach( TTLA T, XA )
        sXA += T.sparam( "XA" );

    foreach( TTLA T, iXA )
        siXA += T.sparam( "iXA" );

    foreach( TTLD T, XD )
        sXD += T.sparam( "XD" );

    foreach( TTLD T, iXD )
        siXD += T.sparam( "iXD" );

    foreach( XBF B, BF )
        sBF += B.sparam();

    if( inarow >= 0 ) {

        if( inarow < 1 )
            inarow = 1;

        sinarow = QString(" -inarow=%1").arg( inarow );
    }

    if( velem.size() )
        ssuper = QString(" -supercat=%1").arg( formatElems() );

    if( !opar.isEmpty() )
        sodir = QString(" -dest=%1").arg( opar );
    else
        out_prb_fld = false;

    sCmd =
        QString(
            "CatGT%1%2%3%4%5%6%7%8%9%10%11%12%13%14%15%16%17%18"
            "%19%20%21%22%23%24%25%26%27%28%29%30%31%32%33%34%35")
        .arg( sreq )
        .arg( sgt )
        .arg( no_run_fld ? " -no_run_fld" : "" )
        .arg( prb_fld ? " -prb_fld" : "" )
        .arg( prb_miss_ok ? " -prb_miss_ok" : "" )
        .arg( exported ? " -exported" : "" )
        .arg( t_miss_ok ? " -t_miss_ok" : "" )
        .arg( sprbs )
        .arg( ap ? " -ap" : "" )
        .arg( lf ? " -lf" : "" )
        .arg( ni ? " -ni" : "" )
        .arg( prb_3A ? " -prb_3A" : "" )
        .arg( szfil )
        .arg( smaxsecs )
        .arg( sapfilter )
        .arg( slffilter )
        .arg( stshift )
        .arg( sloccar )
        .arg( gblcar ? " -gblcar" : "" )
        .arg( sgfix )
        .arg( schnexc )
        .arg( sSY )
        .arg( sXA )
        .arg( sXD )
        .arg( siSY )
        .arg( siXA )
        .arg( siXD )
        .arg( sBF )
        .arg( sinarow )
        .arg( force_ni_ob ? " -pass1_force_ni_ob_bin" : "" )
        .arg( ssuper )
        .arg( sc_trim ? " -supercat_trim_edges" : "" )
        .arg( sc_skipbin ? " -supercat_skip_ni_ob_bin" : "" )
        .arg( sodir )
        .arg( out_prb_fld ? " -out_prb_fld" : "" );

    Log() << QString("Cmdline: %1").arg( sCmd );

// Probe count adjustment

    if( prb_3A )
        vprb.fill( 0, 1 );

// Set default inarow

    if( inarow < 0 )
        inarow = 5;

// Pass-1 specific finish

    if( velem.isEmpty() )
        return pass1FromCatGT() && makeTaggedDest();

// Pass-2 finish

    velem[0].unpack();

    return makeTaggedDest();
}


// Carve gtlist into {g,ta,tb}.
//
bool CGBL::gt_parse_list( const QString &s )
{
    if( !s.contains( "{" ) || !s.contains( "," ) ) {
        Log() << "Error: gtlist not formatted as elements {gj,tja,tjb}.";
        return false;
    }

// Split into elements

    QStringList el = s.split(
                        QRegExp("^\\s*\\{\\s*|\\s*\\}\\s*\\{\\s*|\\s*\\}\\s*$"),
                        QString::SkipEmptyParts );
    int         ne = el.size();

// Split each element

    for( int ie = 0; ie < ne; ++ie ) {

        QStringList sl = el[ie].split(
                            QRegExp("^\\s+|\\s*,\\s*|\\s+$"),
                            QString::SkipEmptyParts );
        int         ns = sl.size();

        if( ns != 3 ) {
            Log() <<
                QString("Error: gtlist element %1 has bad format.")
                .arg( ie );
            return false;
        }

        // parse indices

        GT3 E;
        E.g  = sl[0].toInt();
        E.ta = sl[1].toInt();
        E.tb = sl[2].toInt();

        if( E.g < 0 ) {
            Log() <<
                QString("Error: gtlist element %1 has illegal g.")
                .arg( ie );
            return false;
        }

        if( E.ta < 0 || E.tb < E.ta ) {
            Log() <<
                QString("Error: gtlist element %1 has illegal t-range.")
                .arg( ie );
            return false;
        }

        gtlist.push_back( E );
    }

    return true;
}


void CGBL::gt_set_tcat()
{
    ta = tb = -1;
}


bool CGBL::gt_is_tcat() const
{
    return ta == -1;
}


bool CGBL::gt_ok_indices() const
{
    if( gtlist.size() ) {

        if( ga != -1 || ta != -2 ) {
            Log() << "Error: Option -gtlist cannot be used with -g and -t options.";
            return false;
        }
    }
    else if( ga < 0 || gb < ga || ta < -1 || tb < ta ) {
        Log() << "Error: Bad g- or t-indices.";
        return false;
    }

    return true;
}


QString CGBL::gt_format_params() const
{
    QString s;
    int     n = gtlist.size();

    if( n ) {
        s = " -gtlist=";
        for( int i = 0; i < n; ++i ) {
            const GT3   &E = gtlist[i];
            s += QString("{%1,%2,%3}").arg( E.g ).arg( E.ta ).arg( E.tb );
        }
    }
    else {
        s = QString(" -g=%1").arg( ga );

        if( gb > ga )
            s += QString(",%1").arg( gb );

        if( ta == -1 )
            s += " -t=cat";
        else {
            s += QString(" -t=%1").arg( ta );
            if( tb > ta )
                s += QString(",%1").arg( tb );
        }
    }

    return s;
}


int CGBL::gt_get_first( int *t ) const
{
    if( gtlist.size() ) {

        const GT3   &E = gtlist[0];
        if( t )
            *t = E.ta;
        return E.g;
    }
    else {
        if( t )
            *t = ta;
        return ga;
    }
}


int CGBL::gt_nIndices() const
{
    int n = gtlist.size();

    if( n ) {
        int S = 0;
        for( int i = 0; i < n; ++i ) {
            const GT3   &E = gtlist[i];
            S += E.tb - E.ta + 1;
        }
        return S;
    }
    else
        return (gb - ga + 1) * (tb - ta + 1);
}


bool CGBL::makeOutputProbeFolder( int g0, int ip )
{
    prb_obase = im_obase;

    if( out_prb_fld ) {

        // Create probe subfolder: dest/catgt_run_g0/run_g0_imec0

        prb_obase += QString("/%1_g%2_imec%3").arg( run ).arg( g0 ).arg( ip );

        if( !QDir().exists( prb_obase ) && !QDir().mkdir( prb_obase ) ) {
            Log() << QString("Error creating dir '%1'.").arg( prb_obase );
            return false;
        }

        // Append run name up to _tcat

        prb_obase += QString("/%1_g%2_tcat").arg( run ).arg( g0 );
    }

    return true;
}


QString CGBL::inFile( int g, int t, t_js js, int ip, t_ex ex, XCT *X )
{
    QString s = inPathUpTo_t( g, js, ip );

// t-index

    QString t_str;

    if( t == -1 )
        t_str = "cat";
    else
        t_str = QString("%1").arg( t );

    return s + t_str + suffix( js, ip, ex, X );
}


QString CGBL::niOutFile( int g0, t_ex ex, XCT *X )
{
    QString s;

    if( aux_obase.isEmpty() )
        s = inPathUpTo_t( g0, NI, 0 ) + "cat";
    else
        s = aux_obase;

    return s + suffix( NI, 0, ex, X );
}


QString CGBL::obOutFile( int g0, int ip, t_ex ex, XCT *X )
{
    QString s;

    if( aux_obase.isEmpty() )
        s = inPathUpTo_t( g0, OB, ip ) + "cat";
    else
        s = aux_obase;

    return s + suffix( OB, ip, ex, X );
}


QString CGBL::imOutFile( int g0, t_js js, int ip, t_ex ex, XCT *X )
{
    QString s;

    if( prb_obase.isEmpty() )
        s = inPathUpTo_t( g0, js, ip ) + "cat";
    else
        s = prb_obase;

    return s + suffix( js, ip, ex, X );
}


// Carve chnexcl into {prb;chn}.
//
bool CGBL::parseChnexcl( const QString &s )
{
    QSet<int>   seen;

    if( !s.contains( "{" ) || !s.contains( ";" ) ) {
        Log() << "Error: chnexcl not formatted as elements {prb;channels}.";
        return false;
    }

// Split into elements

    QStringList el = s.split(
                        QRegExp("^\\s*\\{\\s*|\\s*\\}\\s*\\{\\s*|\\s*\\}\\s*$"),
                        QString::SkipEmptyParts );
    int         ne = el.size();

// Split each element

    for( int ie = 0; ie < ne; ++ie ) {

        QStringList sl = el[ie].split(
                            QRegExp("^\\s+|\\s*;\\s*|\\s+$"),
                            QString::SkipEmptyParts );
        int         ns = sl.size();

        if( ns != 2 ) {
            Log() <<
                QString("Error: chnexcl element %1 has bad format.")
                .arg( ie );
            return false;
        }

        // parse channels

        QVector<uint>   C;
        int             prb = sl[0].toInt();

        if( seen.contains( prb ) ) {
            Log() <<
                QString("Error: chnexcl names probe %1 twice.")
                .arg( prb );
            return false;
        }

        if( !Subset::rngStr2Vec( C, sl[1] ) ) {
            Log() <<
                QString("Error: chnexcl element %1 has bad channel list.")
                .arg( ie );
            return false;
        }

        seen.insert( prb );
        mexc[prb] = C;
    }

    return true;
}


// Carve supercat into {dir,run,g}.
//
bool CGBL::parseElems( const QString &s )
{
// Split into elements

    QStringList el = s.split(
                        QRegExp("^\\s*\\{\\s*|\\s*\\}\\s*\\{\\s*|\\s*\\}\\s*$"),
                        QString::SkipEmptyParts );
    int         ne = el.size();

    if( ne < 2 ) {
        Log() << "Error: Supercat requires at least 2 elements.";
        return false;
    }

// Split each element

    QRegExp re("^(catgt_)?(.*)_g(\\d+)$");
    re.setCaseSensitivity( Qt::CaseInsensitive );

    for( int ie = 0; ie < ne; ++ie ) {

        QStringList sl = el[ie].split(
                            QRegExp("^\\s+|\\s*,\\s*|\\s+$"),
                            QString::SkipEmptyParts );
        int         ns = sl.size();

        if( ns != 2 ) {
            Log() <<
                QString("Error: Supercat element %1 has bad format.")
                .arg( ie );
            return false;
        }

        // Split run

        int i = sl[1].indexOf( re );

        if( i < 0 ) {
            Log() <<
                QString("Error: Supercat element %1 has bad run_ga part.")
                .arg( ie );
            return false;
        }

        bool    catgt_fld       = !re.cap(1).isEmpty(),
                e_no_run_fld    = (catgt_fld ? false : no_run_fld);

        velem.push_back( Elem(
            trim_adjust_slashes( sl[0] ), re.cap(2),
            re.cap(3).toInt(), e_no_run_fld, catgt_fld ) );
    }

    return true;
}


QString CGBL::formatChnexcl()
{
    QString s;

    QMap<int,QVector<uint>>::const_iterator it  = mexc.begin(),
                                            end = mexc.end();

    for( ; it != end; ++it ) {

        s += QString("{%1;%2}")
                .arg( it.key() )
                .arg( Subset::vec2RngStr( it.value() ) );
    }

    return s;
}


QString CGBL::formatElems()
{
    QString s;

    for( int ie = 0, ne = velem.size(); ie < ne; ++ie ) {

        const Elem &E = velem[ie];

        s += QString("{%1,%2%3_g%4}")
                .arg( E.dir )
                .arg( E.catgt_fld ? "catgt_" : "" )
                .arg( E.run ).arg( E.g );
    }

    return s;
}


// Check if pass-1 (non-supercat) input is coming from catgt outut.
// Adjust globals {catgt_fld, run, no_run_fld, g-range}.
//
// Return true if ok.
//
bool CGBL::pass1FromCatGT()
{
    if( run.startsWith( "catgt_", Qt::CaseInsensitive ) ) {

        run = run.remove( 0, 6 );

        if( !run.size() ) {
            Log() << QString("Error: empty run name '%1'.").arg( run );
            return false;
        }

        catgt_fld   = true;
        no_run_fld  = false;
    }

    if( catgt_fld || gt_is_tcat() )
        gb = ga;

    return true;
}


// Form reusable output path substrings.
//
// Output parts:
// There is no (g,t) dependence because output is
// always named for (ga,tcat). However, there may
// be probe (ip) dependence.
//
// Suffices:
// NI can be made now.
// IM has (ip) dependence.
//
// Return true if ok.
//
bool CGBL::makeTaggedDest()
{
    if( !opar.isEmpty() ) {

        QString tag = (velem.isEmpty() ? "catgt_" : "supercat_");
        int     g0  = gt_get_first( 0 );

        im_obase = opar;

        // -------------------------------------------------------
        // Create run subfolder for all streams: dest/catgt_run_g0
        // -------------------------------------------------------

        im_obase += QString("/%1%2_g%3").arg( tag ).arg( run ).arg( g0 );

        if( !QDir().exists( im_obase ) && !QDir().mkdir( im_obase ) ) {
            Log() << QString("Error creating dir '%1'.").arg( im_obase );
            return false;
        }

        // ----------------------------------------------
        // NI/OB file base: dest/catgt_run_g0/run_g0_tcat
        // ----------------------------------------------

        aux_obase = QString("%1/%2_g%3_tcat")
                        .arg( im_obase ).arg( run ).arg( g0 );

        // ---------------
        // IM file base...
        // ---------------

        // If not probe folders, all streams use ni_obase style.
        // Else defer until each im stream to:
        //  + create _imec%1 subfolders
        //  + append run_g0_tcat base

        if( !out_prb_fld )
            im_obase = aux_obase;
    }

    return true;
}


QString CGBL::trim_adjust_slashes( const QString &dir )
{
    QString s = dir.trimmed();

    s.replace( "\\", "/" );
    return s.remove( QRegExp("/+$") );
}


QString CGBL::inPathUpTo_t( int g, t_js js, int ip )
{
    QString srun = QString("%1_g%2").arg( run ).arg( g ),
            s;

// parent dir

    s = inpar + "/";

// run subfolder?

    if( !no_run_fld ) {

        if( catgt_fld )
            s += "catgt_";

        s += QString("%1/").arg( srun );
    }

// probe subfolder?

    if( js >= AP && prb_fld )
        s += QString("%1_imec%2/").arg( srun ).arg( ip );

// run name up to _t

    return s + QString("%1_t").arg( srun );
}


QString CGBL::suffix( t_js js, int ip, t_ex ex, XCT *X )
{
    QString suf;

    if( exported )
        suf = ".exported";

    switch( js ) {
        case NI: suf += ".nidq"; break;
        case OB: suf += QString(".obx%1.obx").arg( ip ); break;
        case AP:
        case LF:
            suf += ".imec";
            if( !prb_3A )
                suf += QString("%1").arg( ip );
            suf += (js == AP ? ".ap" : ".lf");
    }

    switch( ex ) {
        case eBIN: suf += ".bin";  break;
        case eMETA: suf += ".meta"; break;
        case eSY: suf += X->suffix( "SY" ); break;
        case eXD: suf += X->suffix( "XD" ); break;
        case eXA: suf += X->suffix( "XA" ); break;
        case eiSY: suf += X->suffix( "iSY" ); break;
        case eiXD: suf += X->suffix( "iXD" ); break;
        case eiXA: suf += X->suffix( "iXA" ); break;
        case eBFT: suf += X->suffix( "BFT" ); break;
        case eBFV: suf += X->suffix( "BFV" ); break;
    }

    return suf;
}


