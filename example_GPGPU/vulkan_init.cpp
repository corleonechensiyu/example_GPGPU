#include"vulkan_init.h"
#include <fstream>
#include <array>
std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
std::vector<const char*> extensions = { VK_EXT_DEBUG_REPORT_EXTENSION_NAME };

inline auto div_up(uint32_t x, uint32_t y) { return (x + y - 1u) / y; }
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

vulkan_init::vulkan_init(const std::string& fileName):physicalDevice(VK_NULL_HANDLE),compute_queue_family_id(-1),commandBuffer(VK_NULL_HANDLE)
{
    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "compute_shader_test";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "no engine";
    appInfo.engineVersion = 0;
    appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = (uint32_t)layers.size();
    instanceCreateInfo.ppEnabledLayerNames = layers.data();
    instanceCreateInfo.enabledExtensionCount = (uint32_t)extensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);//1
    CHECK_RESULT(result);

    //�����м���GPU
    uint32_t numPhysicalDevices = 0;
    result = vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, nullptr);
    CHECK_RESULT(result);
    std::vector<VkPhysicalDevice> physicalDevices(numPhysicalDevices);
    result = vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, physicalDevices.data());
    CHECK_RESULT(result);

    for (const auto& device : physicalDevices)
    {
        //GPU֧�ֵ�extensions
        uint32_t numExtensions = 0;
        result = vkEnumerateDeviceExtensionProperties(device, nullptr, &numExtensions, nullptr);
        CHECK_RESULT(result);
        std::vector<VkExtensionProperties> extensions(numExtensions);
        result = vkEnumerateDeviceExtensionProperties(device, nullptr, &numExtensions, extensions.data());
        CHECK_RESULT(result);
        /*for (const auto& extension : extensions) {
            std::cout << extension.extensionName << std::endl;
        }*/
        //ȷ��ʹ�õ�GPU
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                compute_queue_family_id = i;
                physicalDevice = device;
            }
            i++;
        }
    }
    //��ӡGPU name
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    std::cout << '\n';
    std::cout << deviceProperties.deviceName << std::endl;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = nullptr;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = compute_queue_family_id;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledLayerCount = (uint32_t)layers.size();
    deviceCreateInfo.ppEnabledLayerNames = layers.data();
    deviceCreateInfo.enabledExtensionCount = 0;   ///�������enabledDeviceExtensions ��VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME
    deviceCreateInfo.ppEnabledExtensionNames = nullptr;
    deviceCreateInfo.pEnabledFeatures = nullptr;

    result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);//2
    CHECK_RESULT(result);

    //Create Buffer
    //��ȡglsl����
    auto ShaderCode = readFile("shader.spv");
    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = nullptr;
    shaderModuleCreateInfo.flags = 0;
    shaderModuleCreateInfo.codeSize = ShaderCode.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(ShaderCode.data());

    result = vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, &shaderModule);//3
    CHECK_RESULT(result);

    //����descriptor set layout (binding )shder.comp�ļ����layout(binding=)
    //layout(std430, binding = 0) buffer lay0 { float arr_y[]; };
    //layout(std430, binding = 1) buffer lay1 { float arr_x[]; };
    VkDescriptorSetLayoutBinding layoutBinding[2];
    layoutBinding[0].binding = 0;
    layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;  //TODO VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    layoutBinding[0].descriptorCount = 1;
    layoutBinding[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBinding[0].pImmutableSamplers = nullptr;

    layoutBinding[1].binding = 1;
    layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBinding[1].descriptorCount = 1;
    layoutBinding[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBinding[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = nullptr;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = 2;
    descriptorSetLayoutCreateInfo.pBindings = layoutBinding;

    result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout);//4
    CHECK_RESULT(result);
    ///Ϊ����ʹ�õ����д洢������������������
    VkDescriptorPoolSize poolSizes;
    poolSizes.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes.descriptorCount = 2;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = nullptr;
    descriptorPoolCreateInfo.flags = 0;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &poolSizes;

    result = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool);//5
    CHECK_RESULT(result);

    ////// Create Command Pool
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = compute_queue_family_id;

    result = vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool);//6
    CHECK_RESULT(result);

    ///Create Pipeline Cahce
    VkPipelineCacheCreateInfo cacheCreateInfo;
    cacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheCreateInfo.pNext = nullptr;
    cacheCreateInfo.flags = 0;
    cacheCreateInfo.initialDataSize = 0;
    cacheCreateInfo.pInitialData = nullptr;

    result = vkCreatePipelineCache(device, &cacheCreateInfo, nullptr, &pipelineCache);//7
    CHECK_RESULT(result);

    //push_constant  shader.comp�ļ���� layout(push_constant) uniform Parameters
    VkPushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushParams);

    // Pipeline layout��shader�ӿڶ���Ϊһ��layout bindings��push constants��
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout);//8
    CHECK_RESULT(result);
    /// Create compute pipeline consisting of a single stage with compute shader.
    ///ʹ��compute shader�����ɵ����׶���ɵļ���ܵ���
    //Specialization constants specialized here.
    //// specialize constants of the shader 
    ///layout(local_size_x_id = 0, local_size_y_id = 1)

    auto specEntries = std::array<VkSpecializationMapEntry, 2>{
        { { 0, 0, sizeof(int)},
            { 1, 1 * sizeof(int), sizeof(int) }}
    };
    auto specValues = std::array<int, 2>{16, 16};
    VkSpecializationInfo specInfo;
    specInfo.mapEntryCount = specEntries.size();
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = specValues.size() * sizeof(int);
    specInfo.pData = specValues.data();
    //ָ��compute shader stage��������ڵ�(main)��specializations

    VkPipelineShaderStageCreateInfo shaderCreateInfo;
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderCreateInfo.pNext = nullptr;
    shaderCreateInfo.flags = 0;
    shaderCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderCreateInfo.module = shaderModule;
    shaderCreateInfo.pName = "main";
    shaderCreateInfo.pSpecializationInfo = &specInfo;

    VkComputePipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stage = shaderCreateInfo;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;

    result = vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, NULL, &pipeline);//9
    CHECK_RESULT(result);

    
}

vulkan_init::~vulkan_init() noexcept
{
    vkDestroyPipeline(device, pipeline, nullptr);//9
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);//8
    vkDestroyPipelineCache(device, pipelineCache, nullptr);//7
    vkDestroyCommandPool(device, commandPool, nullptr);//6
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);//5
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);//4
    vkDestroyShaderModule(device, shaderModule, nullptr);//3
    vkDestroyDevice(device, nullptr);//2
    vkDestroyInstance(instance, nullptr);//1
}

void vulkan_init::bindParameters(VkBuffer& out, const VkBuffer& in, const vulkan_init::PushParams& p) const
{
    VkDescriptorSetAllocateInfo setAllocateInfo;
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.pNext = nullptr;
    setAllocateInfo.descriptorPool = descriptorPool;
    setAllocateInfo.descriptorSetCount = 1;
    setAllocateInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(device, &setAllocateInfo, &descriptorSet);
    CHECK_RESULT(result);

    VkDescriptorBufferInfo bufferDescriptor[2];
    bufferDescriptor[0].buffer = out;
    bufferDescriptor[0].offset = 0;
    bufferDescriptor[0].range = sizeof(float) * (p.width * p.height);
    bufferDescriptor[1].buffer = in;
    bufferDescriptor[1].offset = 0;
    bufferDescriptor[1].range = sizeof(float) * (p.width * p.height);

    VkWriteDescriptorSet writeDescriptorset[2];
    writeDescriptorset[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorset[0].pNext = nullptr;
    writeDescriptorset[0].dstSet = descriptorSet;
    writeDescriptorset[0].dstBinding = 0;
    writeDescriptorset[0].dstArrayElement = 0;
    writeDescriptorset[0].descriptorCount = 1;
    writeDescriptorset[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorset[0].pImageInfo = nullptr; //TODO
    writeDescriptorset[0].pBufferInfo = &bufferDescriptor[0];
    writeDescriptorset[0].pTexelBufferView = nullptr;

    writeDescriptorset[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorset[1].pNext = nullptr;
    writeDescriptorset[1].dstSet = descriptorSet;
    writeDescriptorset[1].dstBinding = 1;
    writeDescriptorset[1].dstArrayElement = 0;
    writeDescriptorset[1].descriptorCount = 1;
    writeDescriptorset[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorset[1].pImageInfo = nullptr; //TODO
    writeDescriptorset[1].pBufferInfo = &bufferDescriptor[1];
    writeDescriptorset[1].pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(device, 2, writeDescriptorset, 0, nullptr);
    // ��command pool ����һ��command buffer
    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    
    result = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    CHECK_RESULT(result);
    // ��ʼ��commands��¼���·���� command buffer�С�
    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = nullptr;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    CHECK_RESULT(result);
    // ��dispatch֮ǰ��һ��pipeline, ��һ��descriptor set.
    // �������������Щ��the validation layer������������档
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0u, 1, &descriptorSet, 0u, NULL);
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(p), &p);

    // ���� compute pipeline����ִ��compute shader.
    // The number of workgroups is specified in the arguments. 
    vkCmdDispatch(commandBuffer, div_up(p.width, 16), div_up(p.height, 16), 1);
    result = vkEndCommandBuffer(commandBuffer);
    CHECK_RESULT(result);
}

void vulkan_init::unbindParameters() const
{
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    VkResult result =  vkResetCommandPool(device, commandPool, 0);
    CHECK_RESULT(result);
    /// Allocate descriptor pool for a descriptors to all storage buffer in use
    VkDescriptorPoolSize poolsize;
    poolsize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolsize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo descriptorpoolCreateInfo;
    descriptorpoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorpoolCreateInfo.pNext = nullptr;
    descriptorpoolCreateInfo.flags = 0;
    descriptorpoolCreateInfo.maxSets = 1;
    descriptorpoolCreateInfo.poolSizeCount = 1;
    descriptorpoolCreateInfo.pPoolSizes = &poolsize;

    result = vkCreateDescriptorPool(device, &descriptorpoolCreateInfo, NULL, &descriptorPool);
    CHECK_RESULT(result);

}

void vulkan_init::run() const
{
    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    // submit the command buffer to the queue and set up a fence.
    // 0 is the queue index in the family, by default just the first one is used
    VkQueue queue;
    vkGetDeviceQueue(device, compute_queue_family_id, 0, &queue);
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = 0;

    // fence makes sure the control is not returned to CPU till command buffer is depleted
    VkFence fence;
    VkResult result = vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
    CHECK_RESULT(result);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    // wait for the fence indefinitely
    result = vkWaitForFences(device, 1, &fence, VK_TRUE, uint64_t(-1));
    CHECK_RESULT(result);

    vkDestroyFence(device, fence, nullptr);

}

void vulkan_init::operator()(VkBuffer& out, const VkBuffer& in, const vulkan_init::PushParams& p) const
{
    bindParameters(out, in, p);
    run();
    unbindParameters();
}
