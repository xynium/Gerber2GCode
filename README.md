
**********************************************************************************************
   ____                 _                     ____     ____    ____               _        
  / ___|   ___   _ __  | |__     ___   _ __  |___ \   / ___|  / ___|   ___     __| |   ___ 
 | |  _   / _ \ | '__| | '_ \   / _ \ | '__|   __) | | |  _  | |      / _ \   / _` |  / _ \
 | |_| | |  __/ | |    | |_) | |  __/ | |     / __/  | |_| | | |___  | (_) | | (_| | |  __/
  \____|  \___| |_|    |_.__/   \___| |_|    |_____|  \____|  \____|  \___/   \__,_|  \___|
                                                                                           

**********************************************************************************************

For converting Gerber files to GCode.
Tested with Gerber file from Kicad

Gerber2Gcode test
will execute the conversion on test.gbr and test.drl if exists and write test.gcode 

file should be metric

test.gcode is ready to plot the pcb on a 3D printer with a pen adapter.

Assumed the pen write a 0.3mm line and a recovering of 0.14mm between 2 lines.
Pen Up position is 4mm down 0mmm
If one wants other preset, see PNGerber.hpp and change #define accordingly.

TODO: When plotting zone sometimes display "odd number of line ..." in such a case
modify the recovering distance a little.

Jean Pierre




