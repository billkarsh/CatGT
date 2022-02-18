#ifndef TOOL_H
#define TOOL_H

#include "IMROTbl.h"
#include "KVParams.h"

#include "fftw3.h"

#include <QVector>
#include <QFileInfo>

struct XCT;
struct XBF;
struct Meta;
class Biquad;
class QFileInfo;

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

class IOClient {
public:
    virtual ~IOClient() {}
    virtual void digital( const qint16 *data, int ntpts ) = 0;
    virtual void neural( qint16 *data, int ntpts ) = 0;
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
        int             ap_in,
        int             ap_out );
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

struct Pass1IO {
    IOClient            &client;
    QFileInfo           &fim;
    Meta                &meta;
    QFileInfo           i_fi;
    QString             o_name;
    QFile               i_f,
                        o_f;
    std::vector<qint16> i_buf,
                        o_buf;
    Biquad              *hipass,
                        *lopass;
    FFT                 fft;
    int                 maxInt,
                        ip,
                        ap_in,
                        ap_out,
                        i_nxt,  // next input middle (samples)
                        i_lim,  // input count (samples)
                        gfix0;  // cur o_buf start in i_buf
    bool                doWrite;
    Pass1IO( IOClient &client, QFileInfo &fim, Meta &meta )
        :   client(client), fim(fim), meta(meta),
            hipass(0), lopass(0),
            i_nxt(0), i_lim(0), doWrite(false)  {}
    virtual ~Pass1IO();
    bool o_open( int g0, int ip, int ap_in = 0, int ap_out = 0 );
    void alloc( bool initfft = false );
    void set_maxInt( int _maxInt )  {maxInt = _maxInt;}
    char* o_buf8()                  {return (char*)&o_buf[0];}
    void run();
    int inputSizeAndOverlap( qint64 &xferBytes, int g, int t );
    bool load( qint64 &xferBytes );
    bool push();
    bool flush();
    bool write( qint64 bytes );
    virtual qint64 _write( qint64 bytes );
    virtual bool zero( qint64 gapBytes, qint64 zfBytes );
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
    void write( const QString &outBin, int g0, int t0, int ip = -1, int ap = 0 );
    void read( int ip, int ap = 0 );
    qint64 smpInpSpan()     {return smpInpEOF - smp1st;}
    qint64 smpOutSpan()     {return smpOutEOF - smp1st;}
    qint64 pass1_sizeRead( int &ntpts, qint64 xferBytes, qint64 bufBytes );
    bool pass1_zeroFill( Pass1IO &io, qint64 gapBytes );
    void pass1_fileDone( int g, int t, int ip, int ap = 0 );
    void pass2_runDone()    {++nFiles;}
private:
    void cmdlineEntry();
    void delPass1Tags();
};

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

bool openOutputBinary( QFile &fout, QString &outBin, int g0, int ip, int ap = 0 );

int openInputFile(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    int         ip,
    int         is,
    int         ap = 0,
    XCT         *X = 0 );

int openInputBinary(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    int         ip,
    int         ap = 0 );

int openInputMeta(
    QFileInfo   &fim,
    KVParams    &kvp,
    int         g,
    int         t,
    int         ip,
    int         ap,
    bool        canSkip );

qint64 p2_checkCounts( const Meta &meta, int ie, int ip, int ap = 0 );

bool p2_openAndCopyFile(
    QFile               &fout,
    Meta                &meta,
    std::vector<BTYPE>  &buf,
    qint64              samps,
    int                 ie,
    int                 ip,
    int                 is,
    int                 ap = 0,
    XCT                 *X = 0 );

bool p2_openAndCopyBFFiles( Meta &meta, qint64 samps, int ie, XBF &B );

#endif  // TOOL_H


