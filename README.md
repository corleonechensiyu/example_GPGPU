# example_GPGPU 
简单实现vulkan的compute shader  
代码主要改写[vuh](https://github.com/Glavnokoman/vuh)项目，简单的实现y=ax+y，使用buffer数据存储类型。  
后续可更改为image数据存储类型，实现简单的矩阵乘法。  
1.代码实现步骤很清晰，包括vulkan初始化都是一步步实现，以及本地数据的加载和GPU上计算的数据输出到本地。  
2.vulkan代码学习当中，有些不足欢迎指正，一起学习。  
3.shader.comp中，怎么使用全局工作组和输入的数据(push_constant)得到id(params.Width*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;)，不是很懂。
##### 参考
[ncnn](https://github.com/Tencent/ncnn)  
[mnn](https://github.com/alibaba/MNN)  
[vuh](https://github.com/Glavnokoman/vuh)  
