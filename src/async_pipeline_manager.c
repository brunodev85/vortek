#include "async_pipeline_manager.h"

VkResult AsyncPipelineManager_waitForCreation(ringid_t ringId, int pipelineCount, VkPipeline* pPipelines) {
    int bufferSize = sizeof(VkResult) + pipelineCount * VK_HANDLE_BYTE_COUNT;
    char inputBuffer[bufferSize];

    if (!RingBuffer_read(globalRingPool[ringId].ring, inputBuffer, bufferSize)) return VK_ERROR_DEVICE_LOST;
    globalRingPool[ringId].acquired = false;

    int result = *(int*)(inputBuffer + 0);
    for (int i = 0, j = sizeof(VkResult); i < pipelineCount; i++, j += VK_HANDLE_BYTE_COUNT) {
        uint64_t pipelineId = *(uint64_t*)(inputBuffer + j);
        VkObject* pipelineObject = VkObject_create(VK_OBJECT_TYPE_PIPELINE, pipelineId);
        if (result == VK_NOT_READY) pipelineObject->tag = VK_HANDLE_NOT_READY;
        pPipelines[i] = VkObject_toHandle(pipelineObject);
    }
    if (result == VK_NOT_READY) result = VK_SUCCESS;
    return (VkResult)result;    
}

bool AsyncPipelineManager_checkIfReady(VkObject* pipelineObject) {
    int bytesSent = vt_send(serverRing, REQUEST_CODE_GET_PIPELINE_READY, &pipelineObject->id, sizeof(uint64_t));
    if (bytesSent != sizeof(uint64_t)) return false;

    char* inputBuffer = NULL;
    int result = vt_recv(clientRing, &inputBuffer, NULL, &globalMemoryPool);
    
    if (result == VK_SUCCESS) {
        uint64_t pipelineId = *(uint64_t*)(inputBuffer + 0);
        pipelineObject->id = pipelineId;
        pipelineObject->tag = NULL;
        return true;
    }
    else return false;
}