# Description and Goal 
- In a tank game, each object, such as tanks and particls continuously move over each frame. 
The computation becomes demanding when the number of objects increase quickly, e.g 20 tanks -> 4000 tanks.
This is because severl execution step is taken inside a frame, e.g. position update, calculate attacking range, and do animation if needed.
After execution step, the updated result has to be rendered on screen, which requires another intense computation of updating each pixel. 
Thus, a combined optimization set below is adopted to speed up the whole computation time. 

# Result
The final speed-up compared to original performance is 12.1 without breaking any behavior. 

# Implenmented approaches

### Grid 
- Instread of going over each pixel, we only need to traverse each grid to calculate tanks in the grid, which improves the speed from 3.3x to 8x;

### The separation of rendering and updating 
- The tie of the rendering coming w/ updating calculation is broken by using manual control flow, which leads to another performance boost from 8x to 11.4x. 

### Multi-threading
- It only slightly helps performance from 11.4x to 12.1x since the overhead of thread creation compensates the benefit of concurrency.

### others 
- Use lookup tables to replace intense calculation function e.g. sqrt 
- Prefer to using power of 2 bit operation 

# More ways in the Future
- GPU by using OpenCL/CUDA 
- SIMD(SSE/AVX)
- Caching

