#ifndef TOOL_H
#define TOOL_H

#include "CGBL.h"

#include "fftw3.h"

struct Meta;
class Pass1;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

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

struct FFT {
    double              delT;
    QVector<double>     flt;
    QVector<int>        ic2grp;
    fftw_complex        *cplx;
    double              *real;
    fftw_plan           pfwd,
                        pbwd;
    static QMutex       fftMtx;
    bool                tshift,
                        filter;
    FFT() : cplx(0), real(0), tshift(false), filter(false)  {}
    virtual ~FFT();
    void init(
        const QFileInfo &fim,
        const Meta      &meta,
        t_js            js_in,
        t_js            js_out );
    void apply(
        qint16          *dst,
        const qint16    *src,
        int             ndst,
        int             nsrc,
        int             nC,
        int             nN );
private:
    void extendRHS( int nsrc );
    void timeShiftChannel( int igrp );
};

struct Meta {
    double      srate;
    qint64      smp1st,
                smpToBeWritten,
                smpWritten,
                maxOutEOF;
    KVParams    kvp;
    int         nC,
                nN,
                smpBytes,
                gLast,      // pass-1 tracking
                tLast,      // pass-1 tracking
                nFiles;
    Meta() : smpToBeWritten(0), smpWritten(0), nC(0), nFiles(0) {}
    void read( const QFileInfo &fim, t_js js, int ip );
    void write(
        const QString   &outBin,
        int             ie,
        int             g0,
        int             t0,
        t_js            js,
        int             ip1,
        int             ip2 );
    void writeSave( const QVector<Save> &vSprb, int g0, int t0, t_js js_out );
    qint64 pass1_sizeRead( int &ntpts, qint64 xferBytes, qint64 bufBytes );
    bool pass1_zeroFill( Pass1 &H, qint64 gapBytes );
    void pass1_fileDone( int g, int t, t_js js, int ip );
    void pass2_runDone()    {++nFiles;}
private:
    void cmdlineEntry();
    void delPass1Tags();
};

struct FOffsets {
// Offset of each input file start within concatenated whole
    QMap<QString,double>            mrate0;
    QMap<QString,QVector<qint64>>   moff;
    QMap<QString,QVector<double>>   mrate;
    QMutex                          offMtx;
    void init( double rate, t_js js, int ip );
    void addOffset( qint64 off, t_js js, int ip );
    void addRate( double rate, t_js js, int ip );
    void dwnSmp( int ip );
    void ct_write();
    void sc_write();
    QString stream( t_js js, int ip );
    void writeEntries( QString file );
};

extern FOffsets gFOff;

struct P1LFCase {
// For each lf-ip, classify {0=true LF, 1=AP->LF, 2=skip}
// These data are written ONLY from pass1entrypoint.
    QMap<int,int>   ip2case;
    void init();
    int getCase( int ip ) const {return ip2case[ip];}
private:
    int lfCaseCalc( int g0, int t0, int ip );
};

extern P1LFCase gP1LFCase;

struct P1EOF {
// For each {g,t} file set, its common (shortest) length
// These data are written ONLY from pass1entrypoint.
    struct GTJSIP {
        int     g, t;
        t_js    js;
        int     ip;
        GTJSIP() : g(0), t(0), js(NI), ip(0)    {}
        GTJSIP( int g, int t, t_js js, int ip )
            :   g(g), t(t), js(js), ip(ip)      {}
        bool operator<( const GTJSIP &rhs ) const;
    };
// ---
    struct EOFDAT {
        double  srate,
                span;
        qint64  bytes,
                smp1st;
        int     smpBytes;
    };
// ---
    QMap<GTJSIP,EOFDAT> id2dat;
    bool init();
    EOFDAT getEOFDAT( int g, int t, t_js js, int ip ) const;
private:
    bool getMeta( bool &isSeg, int g, int t, t_js js, int ip, bool t_miss_ok );
};

extern P1EOF    gP1EOF;

/* ---------------------------------------------------------------- */
/* Helpers -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

struct Job;

class JobWorker : public QObject
{
    Q_OBJECT
private:
    const Job &J;
public:
    JobWorker( const Job &J ) : QObject(0), J(J)    {}
signals:
    void finished();
public slots:
    void run();
};

/* ---------------------------------------------------------------- */
/* Functions ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

void pass1entrypoint();
void supercatentrypoint();

#endif  // TOOL_H


