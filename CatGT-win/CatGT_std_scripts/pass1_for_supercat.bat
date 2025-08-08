:: This is a demo script to do CatGT 'pass 1' runs on two recordings.
:: 
:: The companion script, pass2_for_supercat.bat, runs supercat on these outputs to join them.
::  


@echo off
@setlocal enableextensions
@cd /d "%~dp0"


:: INPUT PARAMS -- EDIT TO MATCH YOUR DATA
::
set dir1=D:\SC048_in
set dir2=D:\SC048_in
set run1=SC048_122920_ex
set run2=SC048_122920_ex2
set g=0
set dest=D:\SC048_out


:: PROCESSING PARAMS -- EDIT TO MATCH YOUR DATA 
:: check probe string (e.g. change to probe=0:1 if you have two probes)
:: if you took your data without probe folders, remove -prb_fld
:: add lffilter if needed
:: if you have no ni, remove the line starting with -ni
:: if you have ni, change edge extractions -- these are just examples
::
set process_params=-t=0,0 ^
-prb_fld -ap -prb=0 ^
-apfilter=butter,12,300,10000 -gblcar -gfix=0.40,0.10,0.02 ^
-out_prb_fld ^
-ni -xa=0,0,1,2.5,3.5,0 -xid=0,0,-1,2,2 -pass1_force_ni_ob_bin ^
-dest=%dest%


echo Starting CatGT runs
set LOCALARGS=-dir=%dir1% -run=%run1% -g=%g% %process_params%
%~dp0CatGT %LOCALARGS%


set LOCALARGS=-dir=%dir2% -run=%run2% -g=%g% %process_params%
%~dp0CatGT %LOCALARGS%


echo Done, see CatGT.log file for details
pause

