Experimental GLSL shaders to respect the hardware limitations of Raspberry Pi or similar devices

Compile with glslc or similar

General Constraints and Limits:

GPU
* No cooperative matrices
* No FP16/INT8 arithmetic or storage
* No integer dot product

Compute
* maxComputeSharedMemorySize: 16 KiB per workgroup
* maxComputeWorkGroupInvocations: 256
* maxComputeWorkGroupSize: [256, 256, 256]
  * Each dimension can go up to 256, but the total threads in a group ≤ 256.
  * (So [256,1,1], [16,16,1], [8,8,4], etc. are legal.)

Buffer & Memory
* maxUniformBufferRange: 1 GiB (max size for a Uniform Buffer Object (UBO))
* maxStorageBufferRange: 1 GiB (maximum size for a Shader Storage Buffer Object (SSBO))
* maxPushConstantsSize: 128 bytes
  * Small compared to discrete GPUs (often 256–512 B+). Use UBOs/SSBOs for larger data.

Descriptors & Sets
* maxBoundDescriptorSets: 16
* Per-stage descriptors:
  * Uniform buffers: 16
  * Storage buffers: 8
* Per-pipeline descriptors:
  * Uniform buffers: 64
  * Storage buffers: 32
    (So [256,1,1], [16,16,1], [8,8,4], etc. are legal.)

Vertex/Fragment I/O
* maxVertexInputAttributes: 16
* maxVertexInputBindings: 16
* maxVertexOutputComponents: 64
* maxFragmentInputComponents: 64

Render Targets
* maxFragmentOutputAttachments: 4
* maxColorAttachments: 8
* Framebuffer size: up to 4096 × 4096
