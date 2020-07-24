#include "vulkan_init.h"
#define random(x) rand() % (x)

VkBufferUsageFlags update_usage(const VkPhysicalDevice& physicalDevice, VkMemoryPropertyFlags properties, VkBufferUsageFlags usage)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    if (deviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        properties == VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        && usage == VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    {
        usage |= VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    return usage;
}

void createBuffer(const VkDevice& device, VkBuffer &buffer,uint32_t bufSize, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferInfo;
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = bufSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.queueFamilyIndexCount = 0;
    bufferInfo.pQueueFamilyIndices = nullptr;

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    CHECK_RESULT(result);
}

uint32_t selectMemory(const VkPhysicalDevice& physicalDevice, const VkDevice& device,
    const VkBuffer& buffer, const VkMemoryPropertyFlags requiredProperties)
{
    VkPhysicalDeviceMemoryProperties pMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &pMemoryProperties);
    const uint32_t memoryCount = pMemoryProperties.memoryTypeCount;
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);
    for (uint32_t memoryIndex = 0; memoryIndex < memoryCount; ++memoryIndex) {
        const uint32_t memoryTypeBits = (1 << memoryIndex);
        const bool isRequiredMemoryType = memReqs.memoryTypeBits & memoryTypeBits;

        const VkMemoryPropertyFlags properties =
            pMemoryProperties.memoryTypes[memoryIndex].propertyFlags;
        const bool hasRequiredProperties =
            (properties & requiredProperties) == requiredProperties;

        if (isRequiredMemoryType && hasRequiredProperties)
            return memoryIndex;
    }
    throw std::runtime_error("failed to select memory with required properties");
}

void allocMemory(const VkPhysicalDevice& physDev, const VkDevice& device
    , const VkBuffer& buf, VkDeviceMemory& memory, uint32_t memory_id)
{
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buf, &memoryRequirements);
    VkMemoryAllocateInfo memAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        memoryRequirements.size,
        memory_id,
    };
    
    VkResult result = vkAllocateMemory(device, &memAllocateInfo, NULL, &memory);
    CHECK_RESULT(result);
}
void dst_buffer(std::vector<float>& hostData, const VkDevice& device, const VkPhysicalDevice& physDevce,
    VkBuffer& buffer, VkDeviceMemory& memory)
{
    auto usage = update_usage(physDevce, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto n_elements = (uint32_t)hostData.size();
    createBuffer(device, buffer, n_elements * sizeof(float), usage);
    auto memoryID = selectMemory(physDevce, device, buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    allocMemory(physDevce, device, buffer, memory, memoryID);
    
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physDevce, &memoryProperties);
    auto _flags = memoryProperties.memoryTypes[memoryID].propertyFlags;
    VkResult result = vkBindBufferMemory(device, buffer, memory, 0);
    CHECK_RESULT(result);
}

void staging_buffer(std::vector<float>& hostData, const VkDevice& device, const VkPhysicalDevice& physDevce,
     VkBuffer& buffer, VkDeviceMemory& memory)
{
    auto usage = update_usage(physDevce, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto n_elements = (uint32_t)hostData.size();
    createBuffer(device, buffer, n_elements * sizeof(float), usage);
    auto memoryID = selectMemory(physDevce, device, buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    allocMemory(physDevce, device, buffer, memory, memoryID);
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physDevce, &memoryProperties);
    auto _flags = memoryProperties.memoryTypes[memoryID].propertyFlags;
    VkResult result = vkBindBufferMemory(device, buffer, memory, 0);
    CHECK_RESULT(result);

    void* pData;
    result = vkMapMemory(device, memory, 0, n_elements * sizeof(float), 0, &pData);
    CHECK_RESULT(result);
    memcpy(pData, hostData.data(), n_elements * sizeof(float));
    vkUnmapMemory(device, memory);
}

void copyBuffer(const VkBuffer& src, VkBuffer& dst, const uint32_t size
    , const VkDevice& device, const VkPhysicalDevice& physDev,uint32_t compute_queue_family_id)
{
    const auto qf_id = compute_queue_family_id;
    ///TODO  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        qf_id,
    };
    VkCommandPool commandPool;
    VkResult result = vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool);
    CHECK_RESULT(result);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        NULL,
        commandPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1,
    };
    VkCommandBuffer commandBuffers;
    result = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffers);
    CHECK_RESULT(result);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                NULL,
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                NULL,
    };
    // Download input buffers to device memory.
    result = vkBeginCommandBuffer(commandBuffers, &commandBufferBeginInfo);
    CHECK_RESULT(result);
    VkBufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffers, src, dst, 1, &copyRegion);
    result = vkEndCommandBuffer(commandBuffers);
    CHECK_RESULT(result);

    VkQueue queue;
    vkGetDeviceQueue(device, qf_id, 0, &queue);

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    CHECK_RESULT(result);
    result = vkQueueWaitIdle(queue);
    CHECK_RESULT(result);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffers);
    vkDestroyCommandPool(device, commandPool, nullptr);

}

void no_data_staging_buffer(std::vector<float>& hostData, const VkDevice& device, const VkPhysicalDevice& physicalDevice,
    VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceMemory& srcMemory, VkDeviceMemory& dstMemory, uint32_t id, uint32_t size) {

    auto usage = update_usage(physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto n_elements = (uint32_t)hostData.size();
    createBuffer(device, srcBuffer, n_elements * sizeof(float), usage);
    auto memoryID = selectMemory(physicalDevice, device, srcBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    allocMemory(physicalDevice, device, srcBuffer, srcMemory, memoryID);
    VkResult result = vkBindBufferMemory(device, srcBuffer, srcMemory, 0);
    CHECK_RESULT(result);
    dst_buffer(hostData, device, physicalDevice, dstBuffer, dstMemory);
    copyBuffer(srcBuffer, dstBuffer, size, device, physicalDevice, id);
}
void fromHost(std::vector<float>& hostData,const VkDevice &device,const VkPhysicalDevice &physicalDevice,
    VkBuffer &srcBuffer,VkBuffer &dstBuffer,VkDeviceMemory &srcMemory,VkDeviceMemory &dstMemory,uint32_t id,uint32_t size)
{
    dst_buffer(hostData, device, physicalDevice, dstBuffer, dstMemory);
    staging_buffer(hostData, device, physicalDevice, srcBuffer, srcMemory);
    copyBuffer(srcBuffer, dstBuffer, size, device, physicalDevice, id);

}
void createtoHostBuffer(std::vector<float>& hostData,const VkDevice& device, const VkPhysicalDevice& physDevce,
    VkBuffer& buffer, VkDeviceMemory& memory)
{
    auto usage = update_usage(physDevce, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto n_elements = (uint32_t)hostData.size();
    createBuffer(device, buffer, n_elements * sizeof(float), usage);
    auto memoryID = selectMemory(physDevce, device, buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    allocMemory(physDevce, device, buffer, memory, memoryID);
    //当缓冲区不再使用时，绑定到缓冲区对象的内存获取会被释放，所以让我们在缓冲区被销毁后释放它们
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physDevce, &memoryProperties);
    auto _flags = memoryProperties.memoryTypes[memoryID].propertyFlags;
    VkResult result = vkBindBufferMemory(device, buffer, memory, 0);
    CHECK_RESULT(result);
}
void toHost(std::vector<float>& tohostData, const VkDevice& device, const VkPhysicalDevice& physDevce,
    VkBuffer& stage_buffer,VkBuffer& y_dstBuffer, VkDeviceMemory& stage_memory, VkDeviceMemory& y_dstmemory, uint32_t id)
{
    createtoHostBuffer(tohostData, device, physDevce, stage_buffer, stage_memory);
    copyBuffer(y_dstBuffer, stage_buffer, static_cast<uint32_t>(tohostData.size() * sizeof(float)), device, physDevce, id);
    //将缓存区内容映射到CPU上
    void* pData;
    VkResult result = vkMapMemory(device, stage_memory, 0, static_cast<uint32_t>(tohostData.size() * sizeof(float)), 0, &pData);
    CHECK_RESULT(result);
    memcpy(tohostData.data(), pData, static_cast<uint32_t>(tohostData.size() * sizeof(float)));
    vkUnmapMemory(device, stage_memory);


}
int main()
{


    const int N = 12;
    const int M = 10;
    const int filterWidth = 3;

    auto input = std::vector<float>(N * N);
    for (size_t i = 0; i < input.size(); i++)
    {
        input[i] = static_cast<float>(random(5));
    }

    auto output = std::vector<float>(M * M);
    auto mask = std::vector<float>(filterWidth* filterWidth, 0.2f);

	vulkan_init f("shader.spv");


    
    VkDevice fdevice;
    VkPhysicalDevice fphysicalDevice;
    ///================input===================
    VkBuffer x_srcbuffer;
    VkDeviceMemory x_srcmemory;
    VkBuffer x_dstbuffer;
    VkDeviceMemory x_dstmemory;
    //===============kernel===============
    VkBuffer mask_srcbuffer;
    VkDeviceMemory mask_srcmemory;
    VkBuffer mask_dstbuffer;
    VkDeviceMemory mask_dstmemory;
    ///==================output========================
    VkBuffer y_srcbuffer;
    VkDeviceMemory y_srcmemory;
    VkBuffer y_dstbuffer;
    VkDeviceMemory y_dstmemory;

    fdevice = f.device;
    fphysicalDevice = f.physicalDevice;
    ////数据初始化 
    //输出output矩阵
    no_data_staging_buffer(output, fdevice, fphysicalDevice, y_srcbuffer, y_dstbuffer, y_srcmemory, y_dstmemory, f.compute_queue_family_id, static_cast<uint32_t>(output.size() * sizeof(float)));
    //输入input矩阵
    fromHost(input, fdevice, fphysicalDevice, x_srcbuffer, x_dstbuffer, x_srcmemory, x_dstmemory, f.compute_queue_family_id, static_cast<uint32_t>(input.size() * sizeof(float)));
    //输入kernel矩阵
    fromHost(mask, fdevice, fphysicalDevice, mask_srcbuffer, mask_dstbuffer, mask_srcmemory, mask_dstmemory, f.compute_queue_family_id, static_cast<uint32_t>(mask.size() * sizeof(float)));
    

    
    ///数据绑定
    f(y_dstbuffer, x_dstbuffer,mask_dstbuffer, {N,M,filterWidth });
    //f(y_dstbuffer, x_dstbuffer, { width,height });
    //输出数据
    VkBuffer out_buffer;
    VkDeviceMemory out_memory;

    auto out_tst = std::vector<float>(M * M);
    toHost(out_tst, fdevice, fphysicalDevice, out_buffer, y_dstbuffer, out_memory, y_dstmemory, f.compute_queue_family_id);

    printf("\nArray input:\n");
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++)
            printf("%.3f\t", input[i * N + j]);
        printf("\n");
    }

    printf("\nArray mask:\n");
    for (int i = 0; i < filterWidth; i++) {
        for (int j = 0; j < filterWidth; j++)
            printf("%.3f\t", mask[i * filterWidth + j]);
        printf("\n");
    }
    printf("\nArray output:\n");
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < M; j++)
            printf("%.3f\t", out_tst[i * M + j]);
        printf("\n");
    }

    vkFreeMemory(fdevice, x_srcmemory, nullptr);
    vkDestroyBuffer(fdevice, x_srcbuffer, nullptr);

    vkFreeMemory(fdevice, y_srcmemory, nullptr);
    vkDestroyBuffer(fdevice, y_srcbuffer, nullptr);

    vkFreeMemory(fdevice, mask_srcmemory, nullptr);
    vkDestroyBuffer(fdevice, mask_srcbuffer, nullptr);

    vkFreeMemory(fdevice, x_dstmemory, nullptr);
    vkDestroyBuffer(fdevice, x_dstbuffer, nullptr);

    vkFreeMemory(fdevice, y_dstmemory, nullptr);
    vkDestroyBuffer(fdevice, y_dstbuffer, nullptr);

    vkFreeMemory(fdevice, mask_dstmemory, nullptr);
    vkDestroyBuffer(fdevice, mask_dstbuffer, nullptr);

    vkFreeMemory(fdevice, out_memory, nullptr);
    vkDestroyBuffer(fdevice, out_buffer, nullptr);

	return 0;
}

