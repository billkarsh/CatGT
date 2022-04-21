#ifndef PASS1_H
#define PASS1_H

#include "Tool.h"

class Biquad;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1
{
    friend bool Meta::pass1_zeroFill( Pass1 &H, qint64 gapBytes );

private:
    Biquad              *hipass,
                        *lopass;
    FFT                 fft;
    t_js                js_in,
                        js_out;
    int                 ex0,
                        exLim;

protected:
    QFileInfo           fim;
    Meta                meta;
    QFileInfo           i_fi;
    QFile               i_f,
                        o_f;
    QString             o_name;
    std::vector<qint16> i_buf,
                        o_buf;
    int                 ip,
                        i_nxt,  // next input middle (samples)
                        i_lim,  // input count (samples)
                        gfix0,  // cur o_buf start in i_buf
                        maxInt;
    bool                doWrite;

protected:
    Pass1( t_js js_in, t_js js_out, int ip )
    :   hipass(0), lopass(0), js_in(js_in), js_out(js_out),
        ip(ip), i_nxt(0), i_lim(0), doWrite(false)  {}
    virtual ~Pass1();

    bool o_open( int g0 );
    void initDigitalFields( double rngMax );
    bool openDigitalFiles( int g0 );
    void alloc();
    void fileLoop();
    char* o_buf8()  {return (char*)&o_buf[0];}

    virtual void digital( const qint16 *data, int ntpts );
    virtual void neural( qint16 *data, int ntpts )  {}
    virtual qint64 _write( qint64 bytes );
    virtual bool zero( qint64 gapBytes, qint64 zfBytes );

private:
    int inputSizeAndOverlap( qint64 &xferBytes, int g, int t );
    bool load( qint64 &xferBytes );
    bool push();
    int rem();
    bool flush();
    bool write( qint64 bytes );
};

#endif  // PASS1_H


