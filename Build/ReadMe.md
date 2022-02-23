# CatGT User Manual

## Purpose

+ Optionally join trials with given run_name and index ranges [ga,gb] [ta,tb]...
+ ...Or run on any individual file.
+ Optionally apply demultiplexing corrections.
+ Optionally apply band-pass and global CAR filters.
+ Optionally edit out saturation artifacts.
+ Optionally extract tables of sync waveform edge times to drive TPrime.
+ Optionally extract tables of any other TTL event times to be aligned with spikes.
+ Optionally join the above outputs across different runs (supercat feature).

------

## Install

### (Windows)

1) Copy CatGT-win to your machine, cd into folder.
2) Read this document and the notes in `runit.bat`.

### (Linux)

1) Copy CatGT-linux to your machine, cd into folder.
2) If needed, `> chmod +x install.sh`
3) `> ./install.sh`
4) Read this document and the notes in `runit.sh` wrapper script (required).

### Compatibility (Linux)

- Included libraries are from Ubuntu 16.04 (Xenial).
- Tested with Ubuntu 20.04 and 20.10.
- Tested with Scientific Linux 7.3.
- Tested with Oracle Linux Server 8.3.
- Let me know if it runs on other distributions.

------

## Output

+ Results are placed next to source, named like this, with t-index = 'cat':
`path/run_name_g5_tcat.imec1.ap.bin`.
+ Errors and run messages are appended to `CatGT.log` in the current working directory.

------

## Usage Quick Ref

### (Windows)

`>runit.bat -dir=data_dir -run=run_name -g=ga,gb -t=ta,tb <which streams> [ options ]`

Notes:

+ Since version 1.4.0 runit.bat can, itself, take command-line parameters; you can still edit runit.bat directly if you prefer.
+ It is easiest to learn by editing a copy of `runit.bat`. Double-click on a bat file to run it.
+ Options must not have spaces, generally.
+ File paths and names must not have spaces (a standard script file limitation).
+ In *.bat files, continue long lines using [space][caret]. Like this: `continue this line ^`.
+ Remove all white space at line ends, especially after a caret (^).
+ Read CatGT.log. There is no interesting output in the command window.

### (Linux)

`>runit.sh '-dir=data_dir -run=run_name -g=ga,gb -t=ta,tb <which streams> [ options ]'`

Notes:

+ Enclosing whole linux parameter list in quotes is recommended in general.
+ Enclosing whole linux parameter list in quotes is required for -chnexcl or -supercat.
+ Options must not have spaces, generally.
+ File paths and names must not have spaces (a standard script file limitation).
+ Read CatGT.log. There is no interesting output in the command window.

### Command Line Parameters:

```
Which streams:
-ap                      ;required to process ap streams
-lf                      ;required to process lf streams
-ni                      ;required to process ni stream
-prb_3A                  ;if -ap or -lf process 3A-style probe files, e.g. run_name_g0_t0.imec.ap.bin
-prb=0,3:5               ;if -ap or -lf AND !prb_3A process these probes

Options:
-no_run_fld              ;older data, or data files relocated without a run folder
-prb_fld                 ;use folder-per-probe organization
-prb_miss_ok             ;instead of stopping, silently skip missing probes
-gtlist={gj,tja,tjb}     ;override {-g,-t} giving each listed g-index its own t-range
-t=cat                   ;extract TTL from CatGT output files (instead of -t=ta,tb)
-exported                ;apply FileViewer 'exported' tag to in/output filenames
-t_miss_ok               ;instead of stopping, zero-fill if trial missing
-zerofillmax=500         ;set a maximum zero-fill span (millisec)
-maxsecs=7.5             ;set a maximum output file length (float seconds)
-apfilter=Typ,N,Fhi,Flo  ;apply ap band-pass filter of given {type, order, corners(float Hz)}
-lffilter=Typ,N,Fhi,Flo  ;apply lf band-pass filter of given {type, order, corners(float Hz)}
-no_tshift               ;DO NOT time-align channels to account for ADC multiplexing
-loccar=2,8              ;apply ap local CAR annulus (exclude radius, include radius)
-gblcar                  ;apply ap global CAR filter over all channels
-gfix=0.40,0.10,0.02     ;rmv ap artifacts: ||amp(mV)||, ||slope(mV/sample)||, ||noise(mV)||
-chnexcl={prb;chans}     ;this probe, exclude listed chans from ap loccar, gblcar, gfix
-SY=0,384,6,500          ;extract TTL signal from imec SY (probe,word,bit,millisec)
-XA=2,3.0,4.5,25         ;extract TTL signal from nidq XA (word,thresh1(V),thresh2(V),millisec)
-XD=8,0,0                ;extract TTL signal from nidq XD (word,bit,millisec)
-iSY=0,384,6,500         ;extract inverted TTL signal from imec SY (probe,word,bit,millisec)
-iXA=2,2.0,1.0,25        ;extract inverted TTL signal from nidq XA (word,thresh1(V),thresh2(V),millisec)
-iXD=8,0,0               ;extract inverted TTL signal from nidq XD (word,bit,millisec)
-BF=8,2,4,3              ;extract numeric bit-field from nidq XD (word,startbit,nbits,inarow)
-inarow=5                ;extractor antibounce stay high/low sample count
-pass1_force_ni_bin      ;write pass one ni binary tcat file even if not changed
-supercat={dir,run_ga}   ;concatenate existing output files across runs (see ReadMe)
-supercat_trim_edges     ;supercat after trimming each stream to matched sync edges
-supercat_skip_ni_bin    ;do not supercat ni binary files
-dest=path               ;alternate path for output files (must exist)
-out_prb_fld             ;if using -dest, create output subfolder per probe
```

### Parameter Ordering

You can list parameters on the CatGT command line in any order. CatGT
applies them in the logically necessary order. Of particular note, CatGT
applies filter operations in this order:

- Load data
- Apply any specified biquad (time domain)
- Transform to frequency domain
- TShift
- Apply any specified Butterworth filtering
- Transform back to time domain
- Detect gfix transients for later file editing
- Loccar, Gblcar
- Write file
- Apply gfix transient edits to file

------

## Individual Parameter Notes

### dir

The input files are expected to be organized into folders as SpikeGLX
writes them. CatGT will use your hints {-no_run_fld, -prb_fld} to
automatically generate a path from `data_dir` (the parent directory
of all runs) to the files it needs from `run_name` (this run).
Here are some examples:

- Use `-dir=data_dir -run=run_name -no_run_fld` if the data reside directly
within data_dir without any run folder, as was true in early 3A software,
or if you copied some of your run files without the enclosing run folder.
That is, the data are organized like this:
data_dir/run_name_g0_t0.imec0.ap.bin.

- Use `-dir=data_dir -run=run_name` if you did not select probe folders in
SpikeGLX, that is, the probe data are all at the same level as the NI data
without probe subfolders as in this example:
data_dir/run_name_g0/run_name_g0_t0.imec0.ap.bin.

- Use `-dir=data_dir -run=run_name -prb_fld` if you did select probe folders
in SpikeGLX so that the data from each probe lives in a separate folder
inside the run folder as demonstrated here:
data_dir/run_name_g0/run_name_g0_imec0/run_name_g0_t0.imec0.ap.bin.

Use option `-prb_miss_ok` when run output is split across multiple drives.

### run_name

The input run_name is a base (undecorated) name without g- or t-indices.

### Stream identifiers `{-ap, -lf, -ni}`

In a given run you might have saved a variety of stream/file types
{ap.bin, lf.bin, ni.bin}. Use the `{-ap, -lf, -ni}` flags to indicate
which streams within this run you wish to process.

Note that 2.0 probes output only full-band data with files named ap.bin.
Unlike 1.0 probes there isn't a separate lf band for 2.0. However, if the
following are true:

- The current probe is a 2.0 type.
- -lf is set.
- -lffilter is set (include the low-pass corner!),

then CatGT will create a filtered and downsampled (2500 Hz) lf.bin/meta
file set from the ap.bin stream data.

### prb (which probe(s))

This designates which probes to process. Probe indices are assigned by
SpikeGLX and always start at zero. Note that if you selected the probe
folders box in SpikeGLX, the data for probe 7 would be output to a
subfolder like this: data_dir/run_name_g0/run_name_g0_imec7.

Examples:

- Use `-prb=0` if your run contains one probe only.
- Use `-prb=2:4` to process probes {2,3,4}.
- Use `-prb=1,3:5` to do probes {1,3,4,5} (skip 2).

### Index range (g-, t- concatenation)

#### Background

During a SpikeGLX run the data samples from the hardware are enqueued into
history streams, one stream for each probe and one for NI data. There are
several options for writing data files while a run is in progress. For
example, all of the data can be saved in a continuous manner, which would
produce a single file named `run_name_g0_t0`. As another example, the
`Enable/Disable Recording` (gate control) button might be pressed several
times creating distinct file-writing epochs, each of which gets its own
`g-index`, e.g., {`run_name_g0_t0`, `run_name_g1_t0`, `run_name_g3_t0`, ...}.
Finally, within each open gate epoch, SpikeGLX can write a programmed
sequence of triggered files, incrementing the `t-index` for each of these,
e.g. {`run_name_g7_t0`, `run_name_g7_t1`, `run_name_g7_t2`, ...}. Note that
triggered sequences share a common run_name and g-index. Note too that each
time the gate reopens, the g-index is advanced and the selected trigger
program will start over again beginning with index t0. In all of these
examples the hardware remains in the running state and file data are being
drawn from the shared underlying history streams. That allows files from
the same run (run_name) to be sewn back together so as to preserve the
timing in the original experiment.

#### Usage notes

CatGT can concatenate files together that come from the same run. That is,
the files have the same base run_name, but may have differing g- and
t-indices.

- Example `-g=0` (or `-g=0,0`): specifies the single g-index `0`.
- Example `-t=5` (or `-t=5,5`): specifies the single t-index `5`.
- Example `-g=1,4`: specifies g-index range `[1,4] inclusive`.
- Example `-t=0,100`: specifies t-index range `[0,100] inclusive`.

When a g-range [ga,gb] and/or a t-range [ta,tb] are specified, concatenation
proceeds like two nested loops:

```
    foreach g in range [ga,gb] {

        // put all the t's together for this g...

        foreach t in range [ta,tb] {

            find input FILE with this g and t

            if found {

                // compare FILE 'firstSample' metadata item
                // to the last sample index in the output...

                if firstSample immediately follows the output
                    append FILE to output
                else if firstSample is larger (a gap)
                    zero-fill gap, then append FILE to output
                else if firstSample is smaller (overlap)
                    move FILE pointer beyond overlap, then append remainder
            }
            else if option t_miss_ok is specified
                fill gap with zeros
            else
                stop processing this stream
        }
    }
```

#### Using CatGT output files as input for an extraction pass

Operate on CatGT output files (in order to do TTL extraction) by
setting the -t parameter to: `-t=cat`. Note that you must specify
the single `ga` that labels that tcat file. (More on this in the TTL
extraction notes below).

#### Running CatGT on nonstandard file names

This can be done easily by creating symbolic file links that use the
established SpikeGLX g/t naming conventions.

##### (Windows)

1.	Create a folder, e.g. 'ZZZ', to hold your symlinks;
it acts like a containing run folder. You can make either a flat
folder organization or a standard SpikeGLX hierarchy; adjust the
CatGT parameters accordingly.
2.	Create a .bat script file, e.g. 'makelinks.bat'.
3.	Edit makelinks.bat, adding entries for each bin/meta file pair like this:

```
mklink <...ZZZ\goodname_g0_t0.imec0.ap.bin> <path\myoriginalname.bin>
mklink <...ZZZ\goodname_g0_t0.imec0.ap.meta> <path\myoriginalname.meta>
```

> Set the g/t indices to describe the concatenation order you want.

4.	Close makelinks.bat.
5.	Right-click on makelinks.bat and select `Run as administrator`.

##### (Linux)

1.	Create a folder, e.g. 'ZZZ', to hold your symlinks;
it acts like a containing run folder. You can make either a flat
folder organization or a standard SpikeGLX hierarchy; adjust the
CatGT parameters accordingly.
2.	Create a .sh script file, e.g. 'makelinks.sh'.
3.	Edit makelinks.sh, adding entries for each bin/meta file pair like this:

```
#!/bin/sh

ls -s <path/myoriginalname.bin> <...ZZZ/goodname_g0_t0.imec0.ap.bin>
ls -s <path/myoriginalname.meta> <...ZZZ/goodname_g0_t0.imec0.ap.meta>
```

> Set the g/t indices to describe the concatenation order you want.

4.	Close makelinks.sh, set its executable flag, run it.

### Missing files and gap zero-filling

You can control how CatGT works during file concatenation when one or more
of your input files is missing, or, if input file `N+1` starts later in
time than the end of input file `N`; what we term a "gap" in the recording.

If you do not use the `-t_miss_ok` option, the default behavior is to
require all files in the series to be present. Processing of a stream
will stop if a binary or meta file is not found. If the expected file
set is found but there is a gap between the files, the gap is filled
with zeros for all channels. If instead, adjacent files overlap in
time, the overlap region is represented just once in the output file.

If you include `-t_miss_ok` in the command line, then processing does
not stop. Rather, the entire missing file (or run of consecutive
missing files) is counted as an extended gap. The gap is replaced by
zeros when a next expected file set is found.

By default, CatGT zero-fills gaps so as to precisely preserve the real
world duration of the recording. This enables the spikes and other TTL
events that are present in the output file to be temporally aligned with
other recorded data streams in the experiment.

However, you might not be interested in aligning the data to other streams,
so feel that zeros in the output are wasted space. Moreover, some spike
sorting programs are known to crash because they can not handle long spans
of time with no detected spikes. Option `zerofillmax` allows you to set an
upper limit on the span of zeros that can be inserted.

For example, `zerofillmax=500` directs that gaps whose true length is
<= 500 ms are filled by the equivalent-length span of zeros, but longer
spans are capped at 500 ms of zeros.

Setting `zerofillmax=0` specifies that zero-filling is disabled.

>All detected gaps are noted in the CatGT log file. The log entries
indicate the time (samples from file start) in the output file that
the gap starts, the true length of the gap in the original file set,
and the length of the zero-filled span in the output file.

### Output files

- New .bin/.meta files are output only in these cases:
    1. A range of files is to be concatenated, that is, (gb > ga) or (tb > ta).
    2. If filters are applied, so the binary data are altered.

- If you do not specify the `-dest` option, output files are stored in the
same folder as their input files.

- If a range of g-indices was specified `-g=ga,gb` the data are all written
to the `ga` folder. Such output files look like
`path/run_name_g2_tcat.imec1.ap.bin`, that is, the g-index is set to the
lowest specified input g-index, and the t-index becomes `tcat` to indicate
this file contains a range.

- The `tcat` naming convention is used even if a range in g is specified,
e.g., `-g=0,100`, but there is just one t-index for each gate `t=0`.

- The `-dest=myPath` option will store the output in a destination folder
of your choosing. It will further create an output subfolder for the run
having a `catgt_` tag: `myPath/catgt_run_name_ga`.

- A meta file is also created for each output binary, e.g.:
`path/run_name_g5_tcat.imec1.ap.meta`.

- The meta file also gets `catGTCmdlineN=<command line string>`.

- The meta item e.g., `catNFiles`, indicates count of concatenated files.

- The meta item e.g., `catGVals=0,1`, indicates range of g-indices used.

- The meta item e.g., `catTVals=0,20`, indicates range of t-indices used.

- CatGT creates an output file:
`output_path/run_ga_ct_offsets.txt`.
This tablulates, for each stream, where the first sample of each input
file is relative to the start of the concatenated output file. It records
these offsets in units of samples, and again in units of seconds on that
stream's clock.

### gtlist option

This option overrides the `-g=` and `-t=` options so that you can specify
a separate t-range for each g-index. Specify the list like this:

`-gtlist={g0,t0a,t0b}{g1,t1a,t1b}...` *(include the curly braces)*.

>With this option the g- and t- values in each list element have to be
integers >= 0. You can't use `t=cat` here.

### apfilter and lffilter options

Digital filtering is separately specified for probe AP and LF bands. CatGT
offers these filter options (xx = {ap, lf}):

- xxfilter=biquad,2,Fhi,Flo ; order-2 band-pass
- xxfilter=biquad,2,Fhi,0   ; order-2 high-pass
- xxfilter=biquad,2,0,Flo   ; order-2 low-pass

The biquad is a second order time-domain filter (the order parameter is
actually ignored as it must be 2). Our biquad band-pass is implemented
as a high-pass followed by a low-pass. We apply all biquads in the forward
direction only, making this a causal filter. There is always some phase
error associated with causal filtering. This shouldn't disrupt the ability
to distinguish waveforms from one another in spike sorting, yet the shapes
will differ somewhat from their unfiltered counterparts. This had been the
default type of filtering applied in CatGT through version 2.1.

- xxfilter=butter,N,Fhi,Flo ; order-N band-pass
- xxfilter=butter,N,Fhi,0   ; order-N high-pass
- xxfilter=butter,N,0,Flo   ; order-N low-pass

Our Butterworth filters are implemented in the frequency domain. As such
they are always acausal (zero phase error). The rate of roll-off of the
FFT implementation is about a factor of two slower than in the time domain.
For example, to match the result of a single pass (forward-only) order-3
Butterworth (as per MATLAB filter()), specify order 6 here. To match
forward-backward time-domain filtering with an order-3 (as per MATLAB
filtfilt()), which doubles the effective order, specify order 12 here.

#### no_tshift option

Imec probes digitize the voltages on all of their channels during each
sample period (~ 1/(30kHz)). However, the converting circuits (ADCs) are
expensive in power and real estate, so there are only a few dozen on the
probe and they are shared by the ~384 channels. The channels are organized
into multiplex channel groups that are digitized together, consequently each
group's actual sampling time is slightly offset from that of other groups.

CatGT automatically applies an operation we call `tshift` to undo the
effects of multiplexing by temporally aligning channels to each other. Note
that the "shift" is smaller than one sample so file sizes do not change.
Rather, the amplitude is redistributed among existing samples. Tshift
improves the results of operations that compare or combine different
channels, such as global CAR filtering or whitening. The FFT-based correction
method was proposed by Olivier Winter of the International Brain Laboratory.

Note that tshift and band-pass filtering should always be done on Neuropixel
probe data. The issue is only whether these are applied by CatGT or by some
other component of your analysis pipeline.

- Use option `-no_tshift` to disable CatGT's automatic tshift.

### loccar option

- Do CAR common average referencing on an annular area about each site.
- Specify an excluded inner radius (in sites) and an outer radius.
- Use a high-pass filter also, to remove DC offsets.

### gblcar option

- Do CAR common average referencing using all channels.
- Note that `-gblcar` tends to cancel LFP band signal.
- Use a high-pass filter also, to remove DC offsets.
- No, filter options `-loccar` and `-gblcar` don't make sense together.

### gfix option

Light or chewing artifacts often make large amplitude excursions on a
majority of channels. This tool identifies them and cuts them out,
replacing with zeros. You specify three things.

1. A minimum absolute amplitude in mV (zero ignores the amplitude test).
2. A minimum absolute slope in mV/sample (zero ignores the slope test).
3. A noise level in mV defining the end of the transient.

- Yes, `-gblcar` and `-gfix` make sense used together.

#### Tuning gfix parameters

Use the SpikeGLX FileViewer to select appropriate amplitude and slope
values for a given run. Be sure to turn high-pass filtering ON and spatial
`<S>` filters OFF to match the conditions the CatGT artifact detector will
use. Zoom the time scale (ctrl + click&drag) to see the individual sample
points and their connecting segments. Set the slope this way: Zoom in on
the artifact initial peak, the points with greatest amplitude.
Suppose consecutive points {A,B,C,D} make up the peak and {B,C,D} exceed
the amplitude threshold. Then there are three slopes {B-A,C-B,D-C}
connecting these points. Select the largest value. That is, set the
slope to the fastest voltage change near the peak. An artifact will
usually be several times faster than a neuronal spike.

### chnexcl option

Use this option to prevent bad channels from corrupting calculations over
mixtures of channels, such as the spatial filters {loccar, gblcar, gfix}.

The option `-chnexcl={probe;chan_list}{probe;chan_list}...` takes a list of
elements *(include the curly braces)* that specify a probe index; and a
list of channels to exclude for that probe. Channel lists are specified
like page lists in a printer dialog, `1,10,40:51` for example. Be careful
to use a semicolon (;) between probe and channel list, and use only commas
and colons (,:) within your channel lists.

Note that the CatGT spatial filters honor the metadata item `~snsShankMap`.
The shank map has an entry for each saved channel that describes the
(shank, col, row) where its electrode resides on the shank, and a fourth
0/1 value, `use flag`, indicating if the channel should be used in spatial
filtering. The chnexcl data force the corresponding use flags to zero
before the filters are applied, and the modified `~snsShankMap` is written
to the CatGT output metadata.

### TTL (digital) extractions

A positive TTL pulse:
1. starts at low baseline (below threshold)
2. has a leading/rising edge (crosses above threshold)
3. (optionally) stays high/deflected for a given duration
4. has a trailing/falling edge (crosses below threshold).

There are three positive pulse extractors that each make a report/file of
the times (seconds) of the leading edges of matched pulses. They differ
mainly in the type of channel that they scan, although analog signals
need some additional amplitude parameters.

- SY: Finds positive pulses in a probe SY channel.
- XA: Finds positive pulses in an NI analog channel.
- XD: Finds positive pulses in an NI digital channel.

An inverted TTL pulse:
1. starts at high baseline (above threshold)
2. has a leading/falling edge (crosses below threshold)
3. (optionally) stays low/deflected for a given duration
4. has a trailing/rising edge (crosses above threshold).

There are three inverted pulse extractors that each make a report/file of
the times (seconds) of the leading edges of matched pulses. They differ
mainly in the type of channel that they scan, although analog signals
need some additional amplitude parameters.

- iSY: Finds inverted pulses in a probe SY channel.
- iXA: Finds inverted pulses in an NI analog channel.
- iXD: Finds inverted pulses in an NI digital channel.

For brevity we discuss only the positive pulse extractors. The inverted
pulse versions work exactly the same way. Just keep in mind that inverted
pulses have a high baseline level and deflection toward low values.

>It may be helpful to review the organization of words and bits in data
streams in the
[SpikeGLX User Manual](https://github.com/billkarsh/SpikeGLX/blob/master/Markdown/UserManual.md#channel-naming-and-ordering).

#### XA channels

XA are analog-type NI channels. The signal is parametrized by:

- Index of the word in the stored data file
- Primary TTL threshold-1 (V)
- Optional more stringent threshold-2 (V)
- Milliseconds duration

If your signal looks like clean square pulses, set threshold-2 to be closer
to baseline than threshold-1 to ignore the threshold-2 level and run more
efficiently. For noisy signals or for non-square pulses set threshold-2 to
be farther from baseline than theshold-1 to ensure pulses attain a desired
deflection amplitude. Using two separate threshold levels allows detecting
the earliest time that pulse departs from baseline (threshold-1) and separately
testing that the deflection is great enough to be considered a real event
and not noise (threshold-2).

> SpikeGLX MA channels can also be scanned with the `-XA` option.

#### SY and XD channels

SY (imec), XD (NI) are digital-type channels. The signal is parametrized by:

- Index of the word in the stored data file (or -1, see below)
- Index of the bit in the word
- Milliseconds duration.

>If the SY word is -1, the index of the last word in the stream is
used because that's where the SYNC word appears in standard SpikeGLX
binaries.

>If the XD word is -1, the index of the last word in the stream is
used because that follows all of the analog channels, and most users
have no more than 16 digital lines.

#### **Common to all extractions**

- All indexing is zero-based.

- Milliseconds duration means the signal must remain deflected from baseline for that long.

- Milliseconds duration can be zero to specify detection of all leading edges regardless of pulse duration.

- Milliseconds duration default precision (tolerance) is **+/- 20%**.
    * Default tolerance can be overridden by appending it in milliseconds as the last parameter for that extractor.
    * Each extractor can have its own tolerance.
    * E.g. `-XD=8,0,100`   seeks pulses with duration in default range [80,120] ms.
    * E.g. `-XD=8,0,100,2` seeks pulses with duration in specified range [98,102] ms.

- A given channel or even bit could encode two or more types of pulse that
have different durations, E.g... `-XD=8,0,10 -XD=8,0,20` scans and reports
both 10 and 20 ms pulses on the same line.

- Each option, say `-SY=0,384,6,500`, creates an output file whose name
reflects the parameters, e.g., `run_name_g0_tcat.imec0.ap.SY_384_6_500.txt`.

- The threshold is not encoded in the `-XA` filename; just word and milliseconds.

- The files report the times (s) of leading edges of detected pulses;
one time per line, `\n` line endings.

- The time is relative to the start of the stream in which the pulse is detected (native time).

#### inarow option

All of the TTL extractors use edge detection. By default, when a signal
crosses from low to high, it is required to stay high for at least 5
samples. Similarly, when crossing from high to low the signal is required
to stay low for at least 5 samples. This requirement is applied even when
specifying a pulse duration of zero, that is, it is applied to any edge.
This is done to guard against noise.

You can override the count giving any value >= 1.

### Bit-field (BF) extraction

The -XD and -iXD options treat each bit of an ni XD word as an individual
line. In contrast, the -BF option interprets a contiguous group of XD bits
as a non-negative n-bit binary number. The -BF extactor reports value
transitions: the newest value and the time it changed, in two separate files.
The parameters are:

* **word**: the word to scan in the ni stream (or -1 for last word),
* **startbit**: lowest order bit included in group (range [0..15]),
* **nbits**: how many bits belong to group (range [1..<16-startbit>]).
* **inarow**: a real value has to persist this many samples in a row (1 or higher).

In the following examples we set inarow=3.

For example, to interpret all 16 bits of word 5 as a number, set -BF=5,0,16,3.
To interpret the high-byte as a number, set -BF=5,8,8,3. To interpret bits
{3,4,5,6} as a four-bit value, set -BF=5,3,4,3. You can specify multiple -BF
options on the same command line. The words and bits can overlap.

Each -BF option generates two output files, named according to the
parameters (excluding inarow), for example:

* `run_name_g0_tcat.imec0.ap.BFT_5_3_4.txt`,
* `run_name_g0_tcat.imec0.ap.BFV_5_3_4.txt`.

The two files have paired entries. The `BFV` file contains the decoded
values, and the `BFT` file contains the time (seconds from file start)
that the field switched to that value.

### -t=cat defer extraction to a later pass

Option `-t=cat` allows you to concatenate/filter the data in a first pass
and later extract TTL events from the output files which are now named
`tcat`.

>NOTE: If the files to operate on are now in an output folder named
`catgt_run_name` then *DO PUT* tag `catgt_` in the `-run` parameter
like example (2) below:

>NOTE: Second pass is restricted to TTL/BF extraction. An error is flagged
if the second pass specifies any concatenation or filter options.

**Examples**

- (1) Saving to native folders --
    + Pass 1: `>CatGT -dir=aaa -run=bbb -g=ga,gb -t=ta,tb`.
    + Pass 2: `>CatGT -dir=aaa -run=bbb -g=ga -t=cat`.

- (2) Saving to dest folders --
    + Pass 1: `>CatGT -dir=aaa -run=bbb -g=ga,gb -t=ta,tb -dest=ccc`.
    + Pass 2: `>CatGT -dir=ccc -run=catgt_bbb -g=ga -t=cat -dest=ccc`.

------

## Supercat Multiple Runs

You may want to concatenate the output {bin, meta, extractor} files from
two or more different CatGT runs, perhaps, to spike-sort them jointly and
look for persistent units over several days. You can do that in a two-step
process by (1) running CatGT normally, `"pass 1"`, for each of the separate
runs to generate their tcat-tagged output files, and then (2) running CatGT
on those tcat outputs `"pass 2"` using the `-supercat` option as described
in this section.

## --- Building The Supercat Command Line ---

### supercat option

The new option `-supercat={dir,run_ga}{dir,run_ga}...` takes a list of
elements *(include the curly braces)* that specify which runs to join and
in what order (the order listed). Remember that CatGT lets you store run
output either in the native input folders or in a separate dest folder.
Here's how to interpret and set a supercat element depending on how you
did that CatGT run:

**Example**

- (1) Saved to native folders **with run folder** --
    + dir:    The parent directory of the run folder.
    + run_ga: The name of the run folder **including g-index**.

- (2) Saved to native folders **without run folder (-no_run_fld option)** --
    + dir:    The parent directory of the data files themselves.
    + run_ga: The run_name and g-index parts of the tcat output files.

- (3) Saving to dest folders --
    + dir:    The parent directory of the catgt_run_ga folder.
    + run_ga: 'catgt_' tagged folder name, e.g. **catgt_myrun_g7**.

>Note that if `-no_run_fld` is used, it is applied to all elements.

>Note that in linux, curly braces will be misinterpreted, unless you
enclose the whole parameter list in quotes:<br>
> **> runit.sh 'my_params'**

### supercat_trim_edges option

When SpikeGLX writes files, the first samples written are aligned as closely
as possible across each of the streams, either using elapsed time, or using
sync if enabled for the run. However, the trailing edges of the files,
that is, the last samples written, are not tightly controlled unless you
selected a trigger mode that sets a fixed time span for the files. Said
another way, the starts of files in a run are aligned, but the lengths
of the files are ragged (differences of ~thousandths of a second).

By default, supercat just sews the files from different runs together end
to end without regard for the differences in length of different streams.
However, when the supercat_trim_edges option is enabled, supercat does more
work to trim the files so that the different streams stay better aligned and
closer to the same length. In particular, between each pair of adjacent
runs (A) and (B):

- The trailing edge of each stream of (A) is cut at a sync edge (the same
edge in each stream). The edge itself is kept in the output.

- The leading edge of each stream of (B) is cut at a sync edge (the same
edge in each stream). The edge itself is omitted so that the output has
one unambiguous edge at the boundary.

- Any/all extraction files for a given stream are edited/trimmed in tandem
with the stream's binary files.

>This option requires:
>
> - Sync was enabled in SpikeGLX for each run being supercatted.
> - Sync edges are extracted from each stream during pass 1.
> - Option zerofillmax should not be used during pass 1.
> - Option maxsecs is discouraged during pass 1.

>Note that to supercat lf files, we need their sync edges which can only
>be extracted/derived from their ap counterparts:
>
> - Specify (-ap) and sync edge extraction (-SY) during pass 1.

### supercat_skip_ni_bin option & pass1_force_ni_bin

Your first-pass CatGT runs might have extracted edge files but produced
no new binary NI files (that happens if no trial range is specified in
the g- or t-indices). The supercat_skip_ni_bin option reminds supercat
not to process the missing binary files.

On the other hand, you might want to make a supercat of NI binary files
even though you aren't modifying those data in the first pass. In that
case, do the first pass with `pass1_force_ni_bin` which will ensure that
the NI binary files are made and tagged `tcat` so supercat can find them.

>As of version 1.9, any operations on a stream always produce a new
'tcat' meta file so that supercat can later track file lengths.

### supercat (other parameters)

Here's how all the other parameters work for a supercat session...

>Note that each option is global and will apply to all of the supercat elements.

```
Standard:
-dir                     ;ignored (parsed from {dir,run_ga})
-run                     ;ignored (parsed from {dir,run_ga})
-g=ga,gb                 ;ignored (parsed from {dir,run_ga})
-t=ta,tb                 ;ignored (assumed to be t=cat)

Which streams:
-ap                      ;required to supercat ap streams
-lf                      ;required to supercat lf streams
-ni                      ;required to supercat ni stream
-prb_3A                  ;if -ap or -lf supercat 3A-style probe files, e.g. run_name_g0_tcat.imec.ap.bin
-prb=0,3:5               ;if -ap or -lf AND !prb_3A supercat these probes

Options:
-no_run_fld              ;older data, or data files relocated without a run folder
-prb_fld                 ;use folder-per-probe organization
-prb_miss_ok             ;instead of stopping, silently skip missing probes
-gtlist={gj,tja,tjb}     ;ignored (parsed from {dir,run_ga})
-exported                ;apply FileViewer 'exported' tag to in/output filenames
-t_miss_ok               ;ignored
-zerofillmax=500         ;ignored
-maxsecs=7.5             ;ignored
-apfilter=Typ,N,Fhi,Flo  ;ignored
-lffilter=Typ,N,Fhi,Flo  ;ignored
-no_tshift               ;ignored
-loccar=2,8              ;ignored
-gblcar                  ;ignored
-gfix=0.40,0.10,0.02     ;ignored
-chnexcl={prb;chans}     ;ignored
-SY=0,384,6,500          ;required if joining this extractor type
-XA=2,3.0,4.5,25         ;required if joining this extractor type
-XD=8,0,0                ;required if joining this extractor type
-iSY=0,384,6,500         ;required if joining this extractor type
-iXA=2,2.0,1.0,25        ;required if joining this extractor type
-iXD=8,0,0               ;required if joining this extractor type
-BF=8,2,4,3              ;required if joining this extractor type
-inarow=5                ;ignored
-pass1_force_ni_bin      ;ignored
-dest=path               ;required
-out_prb_fld             ;create output subfolder per probe
```

>Note that you need to provide the same extractor parameters that were used
for the individual runs. Although supercat doesn't do extraction, it needs
the parameters to create filenames.

## --- Supercat Behaviors ---

### Zero filling

There is no zero filling in supercat. Joining is done end-to-end. Missing
files cause processing to stop, with one exception: You can legally skip
probes using the `-prb_miss_ok` option. This allows the data to have been
saved in a multidirectory fashion where not all probes will be in a given
run folder.

Otherwise:

- Every run specified in the supercat list is required to exist.
- Every file type {bin, meta, extractor} being joined must exist in each run.

>Note that supercat will check if the channel count matches from run to run
and flag an error if not. However, beyond that, only you know if it makes
any sense to join these runs together.

### supercat output

You must provide an output directory using the `-dest` option. CatGT will
use the `run_ga` parts of the first listed supercat element to create a
subfolder in the dest directory named `supercat_run_ga` and place the
results there.

The output metadata will **NOT** contain any of these Pass-1 tags:

- `catGTCmdlineN : N in range [0..99]`
- `catNFiles`
- `catGVals`
- `catTVals`

The output metadata **WILL** contain these supercat tags:

- `catGTCmdline`
- `catNRuns`

As runs are joined, supercat will automatically offset the times within
extracted edge files. The offset for the k-th listed run is the sum of
the file lengths for runs 0 through k-1.

Supercat creates an output file:
`dest/supercat_run_ga/run_ga_sc_offsets.txt`.
This tablulates, for each stream, where the first sample of each input
"tcat" file is relative to the start of the concatenated output file. It
records these offsets in units of samples, and again in units of seconds
on that stream's clock.

------

## Change Log

Version 2.5

- Add pass-one ct_offsets file.
- Add supercat sc_offsets file.

Version 2.4

- Add option -gtlist.

Version 2.3

- Fix supercat parameter order dependency.
- Add option -pass1_force_ni_bin.

Version 2.2

- Retire option -tshift (tshift on by default).
- Retire option -gbldmx, preferring tshifted -gblcar.
- Retire options {-aphipass, -aplopass, -lfhipass, -lflopass}.
- Add option -no_tshift (tshift on by default).
- Add option -apfilter=Typ,N,Fhi,Flo.
- Add option -lffilter=Typ,N,Fhi,Flo.

Version 2.1

- BF gets inarow parameter.

Version 2.0

- XA seeks threshold-2 even if millisecs=0.

Version 1.9

- Fix link to fftw3 library.
- Remove glitch at tshift block boundaries.
- Option -gfix now exploits -tshift.
- Option -chnexcl now specified per probe.
- Option -chnexcl now modifies shankMap in output metadata.
- Stream option -lf creates .lf. from .ap. for 2.0 probes.
- Fix supercat premature completion bug.
- Supercat observes -exported option.
- Pass1 always writes new meta files for later supercat.
- Add option -supercat_trim_edges.
- Add option -supercat_skip_ni_bin.
- Add option -maxsecs.
- Add option -BF (bit-field decoder).

Version 1.8

- Add option -tshift.
- Add option -gblcar.

Version 1.7

- Suppress linux brace expansion.

Version 1.6

- Fix bug in g-series concatenation.

Version 1.5

- Improved calling scripts.
- Add option -supercat.

Version 1.4.2

- Fix -zerofillmax size tracking.
- Add option -inarow.

Version 1.4.1

- Working/calling dir can be different from installed dir.
- Log file written to working dir.

Version 1.4.0

- Allow g-range concatenation.
- Add option -zerofillmax.
- Options -SY, -XD accept word=-1 (last word).
- SY output files include ap/lf stream identifier.
- Add options -iSY, -iXA, -iXD.

Version 1.3.0

- Support NP1010 probe.

Version 1.2.9

- Uses 3A imro classes.
- Support for UHD-1 and NHP.

Version 1.2.8

- Add option -prb_miss_ok to skip missing probes.

Version 1.2.7

- Fix reporting back of user -XA command line options.
- Add optional tolerance parameter to each extractor.

Version 1.2.6

- CAR filters are applied whole-probe, not shank-by-shank.
- Better command line error messages.

Version 1.2.5

- Fix option -gfix crash.
- Fix -gfix artifacts.
- More accurate -gfix spans.
- Log gfix/second average fix rate.

Version 1.2.4

- New bin/meta output only if concatenating or filtering.
- Reuse output run folder if already exists.
- Add option -t=cat to allow TTL extraction as a second pass.
- Add option -exported to recognize FileViewer export files.

Version 1.2.3

- Better error reporting.
- Add metadata tag catGTCmdlineN.
- Add option -loccar.
- Rename option -gblexcl to -chnexcl.
- More improvements to option -gfix.
- TTL extractors handle smaller widths.

Version 1.2.2

- Improvements to option -gfix.

Version 1.2.1

- Fix option -out_prb_fld.

Version 1.1

- Option -dest creates subfolder run_name_g0_tcat.
- Add option -out_prb_fld.
- Add tag 'fileCreateTime_original' to metadata.

Version 1.0

- Initial release.


_fin_

