# D3D12 practice 
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/a5ced3c359e34a45b04d5579239abf5c)](https://app.codacy.com/manual/sienaiwun/Dx12Practice?utm_source=github.com&utm_medium=referral&utm_content=sienaiwun/Dx12Practice&utm_campaign=Badge_Grade_Dashboard)
[![Build status](https://ci.appveyor.com/api/projects/status/4n54x9aia7tslckr?svg=true)](https://ci.appveyor.com/project/sienaiwun/dx12practice)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/8d155979fc2444bbb56479190020eb14)](https://www.codacy.com/manual/sienaiwun/Dx12Practice?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=sienaiwun/Dx12Practice&amp;utm_campaign=Badge_Grade)

This project is based on Microsfts&copy; [MiniEngine](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine). Besides that I add some toy demos
## Entry
* Open ModelViewer/ModelViewer_VS15.sln
## Computer shader:
* A simple Computer shader example inspaird by [Plantary Gear](https://www.shadertoy.com/view/MsGczV)
![image](https://github.com/sienaiwun/Dx12Practice/blob/master/Images/compute.gif?raw=true)

## Gpu Tiles Culling and Indirect draw:
* Accelerate draw-calls submitions using indirect draw calls generated from GPU-based tiles culling. 
In the following image, each color block is a quad draw-call. The shape is inspaird by [Heart - 2D](https://www.shadertoy.com/view/XsfGRn)
![image](https://github.com/sienaiwun/Dx12Practice/blob/master/Images/IndirectDraw.gif?raw=true)

## Tiled Texture:
* Using D3D12 reserved texture as virtual texture's memory holder.
![vitual textrue](https://github.com/sienaiwun/publicImgs/blob/master/imgs/VirtualTexture/virtual_texture_checkbox_dx12.gif?raw=true)
* Tiled memory visualization.
For the view like this:
![checkbox_vt.png](https://github.com/sienaiwun/publicImgs/blob/master/imgs/VirtualTexture/checkbox_vt.png?raw=true)
Only a small subset of the entire texture is loaded. The actual loaded texture is visualized as:
![mip_chain.gif](https://github.com/sienaiwun/publicImgs/blob/master/imgs/VirtualTexture/mip_chain.gif?raw=true)

## Voxel cone tracing:
Work in progress
