#pragma once
#include <memory>
#include <vector>

#include "volk.h"
#include "Graphic.h"
#include "Voxel.h"
#include "RasterizationPipeline.h"
#include "Buffer.h"
#include "Descriptor.h"

namespace gm {
  class Rasterizer {
    struct PushConstant {
      glm::mat4 projection;
      glm::mat4 view;
      uint32_t  time;
    };

    static std::unique_ptr<Buffer> vertexBuffer;
    static std::unique_ptr<Buffer> indexBuffer;

    //todo currently not very efficient with removing/adding single voxels
    std::unique_ptr<Buffer> instanceBuffer[2];
    uint32_t                instanceCount = 0;

    VkPipelineLayout                       pipelineLayout;
    std::unique_ptr<RasterizationPipeline> pipeline;

    std::unique_ptr<DescriptorPool>      descriptorPool;
    std::unique_ptr<DescriptorSetLayout> setLayout;
    VkDescriptorSet                      descriptorSets[2];

    bool activeIndex = 0;

    void initBuffers();
    void initDescriptors();
    void initPipelineLayout();
    void initPipeline();
    void bindInstances(std::vector<obj::Voxel::Instance> voxels);

  public:
    Rasterizer();

    void        render(Frame frameInfo);
    static void cleanup();
  };
}
