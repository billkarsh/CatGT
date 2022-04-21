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
    void read( t_js js, int ip );
    void write( const QString &outBin, int g0, int t0, t_js js, int ip );
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
    QMap<QString,double>            mrate;
    QMap<QString,QVector<qint64>>   moff;
    void init( double rate, t_js js, int ip );
    void addOffset( qint64 off, t_js js, int ip );
    void dwnSmp( int ip );
    void ct_write();
    void sc_write();
    QString stream( t_js js, int ip );
    void writeEntries( QString file );
};

extern FOffsets gFOff;

/* ---------------------------------------------------------------- */
/* Functions ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

void pass1entrypoint();
void supercatentrypoint();

#endif  // TOOL_H


