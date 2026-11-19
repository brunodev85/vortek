#ifndef VORTEK_ASYNC_PIPELINE_MANAGER_H
#define VORTEK_ASYNC_PIPELINE_MANAGER_H

#include "vortek.h"

extern VkResult AsyncPipelineManager_waitForCreation(ringid_t ringId, int pipelineCount, VkPipeline* pPipelines);
extern bool AsyncPipelineManager_checkIfReady(VkObject* pipelineObject);

#define VK_HANDLE_NOT_READY ((void*)1)

#endif