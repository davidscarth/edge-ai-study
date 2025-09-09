Experimental GLSL shaders to respect the hardware limitations of Raspberry Pi or similar devices

Compile with glslc or similar

Hardware Constraints:
* No cooperative matrices
* No FP16/INT8 arithmetic or storage
* No integer dot product
* 256 workgroup limit
* 16KiB SMEM limit
* 128MiB maxStorageBufferRange Limit
