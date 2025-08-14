AT_CSV=pi5_wide.csv \
./autotune --preset=extended16k_capped \
  --lsz=16x8,16x16,32x8 \
  --Ms=48,64,80 --Ns=96,112,128 \
  --add-tiles=64x96,64x112,64x128 \
  --enable-smem=1



  AT_CSV=pi5_tall.csv \
./autotune --preset=extended16k_capped \
  --lsz=16x8,16x16,32x8 \
  --Ms=96,112,128 --Ns=48,64,80 \
  --add-tiles=96x64,112x64,128x64 \
  --enable-smem=1

  

  AT_CSV=pi5_square.csv \
./autotune --preset=extended16k_capped \
  --lsz=16x8,16x16,32x8 \
  --Ms=48,64,80,96 --Ns=48,64,80,96 \
  --add-tiles=64x64,80x80,96x96 \
  --enable-smem=1
