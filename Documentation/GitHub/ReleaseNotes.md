# Release notes

## 0.2 - 2026-08-17

**BSL improvements**
 * Structs can now hold opaque types such as textures and samplers. They can be nested, passed to functions, and returned from them.

   ```hlsl
   struct CombinedTexture
   {
       Texture2D    tex;
       SamplerState samp;
   };

   float4 SampleHelper(CombinedTexture ct, float2 uv)
   {
       return ct.tex.Sample(ct.samp, uv);
   }
   ```

 * Added support for templated structs and functions, including explicit specialization.

   ```hlsl
   template<typename T>
   struct Pair { T left; T right; };

   template<typename T>
   T Add(T lhs, T rhs) { return lhs + rhs; }

   template<>
   float4 Add(float4 lhs, float4 rhs) { return lhs - rhs; }
   ```

**Shader pipeline**
 * Shaders can be compiled ahead of time and loaded from a prebuilt store, via the new cook tool and the ShaderRegistry.
 * Shader backends are now registered per platform and can load as dynamic libraries, so you can cross-compile shaders for other platforms from a host platform.
 * Program bytecode now carries the descriptor tables.
 * Generated SPIR-V is optimized with SPIRV-Tools.

**GPU work contexts**
 * Added GpuWorkContext, which owns the GPU state for a single thread or fiber.
 * Context owns command buffer pool, parameter set pool, transient scratch memory and a completion tracker.
 * Command buffer submission moved from GpuDevice to GpuWorkContext.
 * Transient memory allocation moved from GpuDevice to GpuWorkContext.
 * TextureUtility and GpuBufferUtility now take a context. RendererMaterial can be used from worker threads through one, and created asynchronously.

 **Native BC shaders**
 * Block compression (BC1/3/4/5/6/7) now run using native shaders on the GPU, for a blazing fast compression
 * Mip-map generation is now also moved to the GPU
 * NVTT is no longer a dependency.

**GPU backend unification**
 * GPU resource tracking and barrier management moved out of the Vulkan backend into the core, so they are now shared by every backend.
 * GPU submit thread, swap chain and vertex input manager moved out of the Vulkan backend into the core, so they are now shared by every backend.
 * All GPU memory allocators now share a common IGpuAllocator interface. 
 * Added GpuTimelineFence, refactor command buffer submission and GPU queues so they utilize them internally.

**Async compute**
 * Compute and transfer work can run on their own queues, in parallel with graphics. 
 * The framework derives the cross-queue synchronization from how each command buffer uses its resources, so no manual semaphore management is needed.

**GUI**
 * CSS supports line height and character spacing, comments, and negative numbers. 
 * CSS parsing reports errors in more detail.
 * Added font families. A family groups the font files that differ only in weight and slant.
 * New Font manager auto-discovers and on-demand loads font families and their faces
 * Added CSS `font-weight` property that accepts `normal`, `bold`, or a number from 100 to 900. 
 * Added CSS `font-style` property that accepts `normal`, `italic` or `oblique`.
 * The CSS `font-family` property now accepts a family name, or a `url()` pointing at a single font file.

**Input**
 * All platform input implementations now sit behind a common IInputBackend interface.
 * Gamepads are detected when connected or disconnected while the application runs.

**File system**
 * Asynchronous file I/O on Windows, Linux and macOS, through platform specific file data streams that replace the fstream based one.
 * Package loading, internal RTTI operations, the buffered bitstream reader and compression all read through the new asynchronous path.
 * The FileSystem::OpenFile API now takes explicit flags for access control and asynchronous access.
 * Files are now opened with correct sharing modes, which makes sharing violations easier to detect. Existing sharing violations fixed.

**Scripting**
 * Added ahead-of-time (AOT) compilation support for C# code.

**Core**
 * MemoryDataStream accepts a custom deleter.
 * Assert dialogs can be disabled, for unattended runs.
 * New console variables dump Vulkan pipeline statistics and pipeline compile times.
 * Added TPool.
 * All platform specific code lives under a single root Platform directory, so a platform and its dependencies stay isolated.
 * Threading primitives and dynamic library handling moved into per-platform headers, so a platform can replace them.
 * Platforms can override compiler and linker flags. Importer builds can be disabled, which consoles do not need.

**Breaking changes**
 * The GUI is now evaluated in linear space. Re-import all UI textures tagged as sRGB.
 * Vulkan 1.2 is now the minimum required version.
 * B3D_ENABLE_TESTS is renamed to B3D_BUILD_TESTS.
 * Old API script export-tag is split into separate Assembly and API tags, because the old tag controlled two unrelated things.

## 0.1 - 2026-05-23

 * Initial release.
