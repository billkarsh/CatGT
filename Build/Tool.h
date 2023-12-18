#ifndef TOOL_H
#define TOOL_H

#include "CGBL.h"

#include "fftw3.h"

struct Meta;
class Pass1;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

typedef long double  BTYPE;

struct FFT {
    double              delT;
    QVector<double>     flt;
    QVector<int>        ic2grp;
    fftw_complex        *cplx;
    double              *real;
    fftw_plan           pfwd,
                        pbwd;
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
    void timeShiftChannel( int igrp );
};

struct Meta {
    double      srate;
    qint64      smp1st,
                smpInpEOF,
                smpOutEOF,
                maxOutEOF;
    KVParams    kvp;
    int         nC,
                nN,
                smpBytes,
                gLast,
                tLast,
                nFiles;
    void read( t_js js );
    void write( const QString &outBin, int g0, int t0, t_js js, int ip );
    void writeSave( int sv0, int svLim, int g0, int t0, t_js js_out );
    qint64 smpInpSpan()     {return smpInpEOF - smp1st;}
    qint64 smpOutSpan()     {return smpOutEOF - smp1st;}
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

struct P1EOF {
// For each {g,t} file set, its common (shortest) length
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
        qint64  bytes;
        int     smpBytes;
    };
// ---
    QMap<GTJSIP,EOFDAT> id2dat;
    bool init();
    qint64 fileBytes(
        const KVParams  &kvp,
        int             g,
        int             t,
        t_js            js,
        int             ip ) const;
private:
    bool getMeta( int g, int t, t_js js, int ip, bool t_miss_ok );
};

extern P1EOF    gP1EOF;

/* ---------------------------------------------------------------- */
/* Functions ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

void pass1entrypoint();
void supercatentrypoint();

#endif  // TOOL_H


