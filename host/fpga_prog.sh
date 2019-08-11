FICMGR="/home/asap/fic/ras_hunga/ficmgr.py"
MK1_4x4="/home/asap/fic/FICSES/FICSESMK1/FICSESMK1_4x4"
MK1_5x5="/home/asap/fic/FICSES/FICSESMK1/FICSESMK1_5x5"
MK2_4x4="/home/asap/fic/FICSES/FICSESMK2/FICSESMK2_4x4"
MK2_5x5="/home/asap/fic/FICSES/FICSESMK2/FICSESMK2_5x5"

$FICMGR -t fic03 fic02 fic01 fic00 -p ${MK1_4x4}/fic_top.bin -pm sm8
#$FICMGR -t fic07 fic06 fic05 fic04 -p ${MK1_4x4}/fic_top.bin -pm sm8
#$FICMGR -t fic11 fic10 fic09 fic08 -p ${MK1_4x4}/fic_top.bin -pm sm8

#$FICMGR -t m2fic03 m2fic02 m2fic01 m2fic00 -p ${MK2_4x4}/ficses_mk2.bin -pm sm16
#$FICMGR -t m2fic07 m2fic06 m2fic05 m2fic04 -p ${MK2_4x4}/ficses_mk2.bin -pm sm16
#$FICMGR -t m2fic11 m2fic10 m2fic09 m2fic08 -p ${MK2_4x4}/ficses_mk2.bin -pm sm16




