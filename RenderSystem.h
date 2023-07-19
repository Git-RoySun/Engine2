#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Pipeline.h"
#include "Object.h"
#include "Device.h"
#include "Camera.h"


namespace vc {
	struct PushConstantData {
		glm::mat4 transform{1.0f};
	};

	struct FrameInfo{
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		Camera& camera;
	};

	class RenderSystem {
		Device& device;
		VkPipelineLayout pipelineLayout;
		std::unique_ptr<Pipeline> pipeline;

		void initPipelineLayout();
		void initPipeline(VkRenderPass renderPass);

	public:
		RenderSystem(Device& device, VkRenderPass renderPass);
		~RenderSystem();

		RenderSystem(const RenderSystem&) = delete;
		RenderSystem& operator=(const RenderSystem&) = delete;

		void renderObjects(FrameInfo info, std::vector<Object>& objects);
	};
}

