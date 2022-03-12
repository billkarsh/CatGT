#ifndef TOOL_H
#define TOOL_H

#include "CGBL.h"
#include "IMROTbl.h"
#include "KVParams.h"

#include "fftw3.h"

#include <QFileInfo>

struct Meta;
class Pass1;
class Biquad;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

typedef long double  BTYPE;

struct CniCfg
{
    enum niTypeId {
        niTypeMN    = 0,
        niTypeMA    = 1,
        niTypeXA    = 2,
        niTypeXD    = 3,
        niSumNeural = 0,
        niSumAnalog = 2,
        niSumAll    = 3,
        niNTypes    = 4
    };
};

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

IMROTbl *getProbe( const KVParams &kvp );

bool getSavedChannels(
    QVector<uint>   &chanIds,
    const KVParams  &kvp,
    const QFileInfo &fim );

bool openOutputBinary( QFile &fout, QString &outBin, int g0, t_js js, int ip );

int openInputFile(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    t_js        js,
    int         ip,
    t_ex        ex,
    XTR         *X = 0 );

int openInputBinary(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    t_js        js,
    int         ip );

int openInputMeta(
    QFileInfo   &fim,
    KVParams    &kvp,
    int         g,
    int         t,
    t_js        js,
    int         ip,
    bool        canSkip );

#endif  // TOOL_H


