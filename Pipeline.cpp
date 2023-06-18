#include "Pipeline.h"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cassert>

namespace vc{
	std::vector<char> Pipeline::readFile(const std::string& filePath) {
		std::ifstream file{filePath, std::ios::ate | std::ios::binary};
		//^ this line will call abort (crash) if file is invalid and not throw an error
		if (!file.is_open()) {
			throw std::runtime_error("Pipeline failed to open file "+ filePath);
		}
		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
	Pipeline::~Pipeline() {
		vkDestroyShaderModule(device.device(), vertShader, nullptr);
		vkDestroyShaderModule(device.device(), fragShader, nullptr);
		vkDestroyPipeline(device.device(), vkPipeline, nullptr);
	};

	Pipeline::Pipeline(Device& device, const std::string& vertPath, const std::string& fragPath, PipelineFixedStageInfo stageInfo) :device{ device }{
		auto vertFile = readFile(vertPath);
		auto fragFile = readFile(fragPath);

		createShaderModule(vertFile, &vertShader);
		createShaderModule(fragFile, &fragShader);

		VkPipelineShaderStageCreateInfo shaderStages[2];
		shaderStages[0] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertShader,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		shaderStages[1] = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragShader,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.vertexBindingDescriptionCount = 0,
				.pVertexBindingDescriptions = nullptr, 
				.vertexAttributeDescriptionCount = 0,
				.pVertexAttributeDescriptions = nullptr,
		};


		VkPipelineViewportStateCreateInfo viewportInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = &stageInfo.viewport,
			.scissorCount = 1,
			.pScissors = &stageInfo.scissor,
		};

		VkGraphicsPipelineCreateInfo pipelineInfo = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &stageInfo.inputAssemblyInfo,
			.pViewportState = &viewportInfo,
			.pRasterizationState = &stageInfo.rasterizationInfo,
			.pMultisampleState = &stageInfo.multisampleInfo,
			.pDepthStencilState = &stageInfo.depthStencilInfo,
			.pColorBlendState = &stageInfo.colorBlendInfo,
			.pDynamicState = nullptr,
			.layout = stageInfo.pipelineLayout,
			.renderPass = stageInfo.renderPass,
			.subpass = stageInfo.subpass,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};

		if (vkCreateGraphicsPipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkPipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create graphics pipeline!");
		}
	}
	
	void Pipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
		VkShaderModuleCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = code.size(),
			.pCode = reinterpret_cast<const uint32_t*>(code.data()),
		};

		if(vkCreateShaderModule(device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("Could not create shader module!");
		}
	}

	PipelineFixedStageInfo Pipeline::defaultPipelineInfo(uint32_t width, uint32_t height) {
		PipelineFixedStageInfo configInfo{};

		configInfo.viewport = {
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(width),
			.height = static_cast<float>(height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};

		configInfo.scissor = {
			.offset = {0, 0},
			.extent = {width, height}
		};

		configInfo.inputAssemblyInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE
		};

		configInfo.rasterizationInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,			
			.depthBiasConstantFactor = 0.0f,	//optional
			.depthBiasClamp = 0.0f,						//optional
			.depthBiasSlopeFactor = 0.0f,			//optional
			.lineWidth = 1.0f,
		};

		configInfo.multisampleInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 1.0f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE,
		};

		configInfo.colorBlendAttachment = {
			.blendEnable = VK_FALSE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask =
				VK_COLOR_COMPONENT_R_BIT |
				VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT |
				VK_COLOR_COMPONENT_A_BIT,
		};

		configInfo.colorBlendInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = 1,
			.pAttachments = &configInfo.colorBlendAttachment,
			.blendConstants = {0.0f,0.0f,0.0f,0.0f},
		};

		configInfo.depthStencilInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {},
			.back = {},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f,
		};

		return configInfo;
	}

	void Pipeline::bind(VkCommandBuffer commandBuffer) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
	}
}