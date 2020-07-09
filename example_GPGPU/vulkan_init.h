#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>

#define CHECK_RESULT(r) do {    \
    if ((r) != VK_SUCCESS) {    \
        printf("result = %d, line = %d\n", (r), __LINE__);  \
        throw;  \
    }   \
} while (0)


class vulkan_init
{
	
	struct PushParams
	{
		uint32_t width;
		uint32_t height;
		float a;
	};
public:
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkShaderModule shaderModule;
	VkDescriptorSetLayout descriptorSetLayout;
	mutable VkDescriptorPool descriptorPool;
	VkCommandPool commandPool;
	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	mutable VkCommandBuffer commandBuffer;
	uint32_t compute_queue_family_id;

public:
	explicit vulkan_init(const std::string& fileName);
	~vulkan_init() noexcept;

	void bindParameters(VkBuffer &out,const VkBuffer &in,const PushParams &p) const;
	void unbindParameters() const;
	void run() const;
	void operator()(VkBuffer& out, const VkBuffer& in, const PushParams& p)const;
	
};

