:: Demo script to run run supercat on output from pass1_for_supercat.bat, 
:: pass1 filters data and performs edge extractions
:: edit the supercat elements to match your data. See 'Building The Supercat Command Line' in the ReadMe for help.
:: change streams (add/remove ni, add lf) and probe string to match your data.
:: add any edge extractors (extractors for sync edges are added automatically).
:: If your data does not have any sync signal, remove -supercat_trim_edges.
:: set dest to your desired output directory
::


@echo off
@setlocal enableextensions
@cd /d "%~dp0"


echo Starting CatGT supercat run
set LOCALARGS=-supercat={D:\SC048_out,catgt_SC048_122920_ex_g0}{D:\SC048_out,catgt_SC048_122920_ex2_g0} ^
-ap -ni -prb=0 -supercat_trim_edges -prb_fld -out_prb_fld ^
-xa=0,0,1,2.5,3.5,0 -xid=0,0,-1,2,2 ^
-dest=D:\SC048_out


%~dp0CatGT %LOCALARGS%


echo Done, see CatGT.log file for details
pause

