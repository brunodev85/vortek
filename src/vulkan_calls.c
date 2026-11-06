#include <sys/mman.h>
#include <pthread.h>

#include "vortek.h"
#include "vortek_serializer.h"
#include "descriptor_update_template.h"
#include "vk_object_pool.h"
#include "vulkan/vk_icd.h"
#include "vulkan/vk_layer.h"

#define MSG_DEBUG_UNIMPLEMENTED_VKCALL "vortek: unimplemented call %s\n"

struct VulkanFunc {
    char* name;
    void* func;
};

static void* findVkDispatchFuncWithName(const char* name);

static pthread_mutex_t vt_call_mutex = PTHREAD_MUTEX_INITIALIZER;

#define VT_CALL_LOCK() pthread_mutex_lock(&vt_call_mutex)
#define VT_CALL_UNLOCK() \
    mp_free(&globalMemoryPool); \
    pthread_mutex_unlock(&vt_call_mutex);

static VkResult waitForPipelineCreation(int pipelineCount, VkPipeline* pPipelines) {
    int numFds, fd;
    char success = 0;
    recv_fds(serverFd, &fd, &numFds, &success, 1);
    VT_CALL_UNLOCK();

    if (!success || numFds != 1) return VK_ERROR_DEVICE_LOST;

    int bufferSize = sizeof(VkResult) + pipelineCount * VK_HANDLE_BYTE_COUNT;
    char inputBuffer[bufferSize];

    int bytesRead = read(fd, inputBuffer, bufferSize);
    CLOSEFD(fd);
    if (bytesRead != bufferSize) return VK_ERROR_DEVICE_LOST;

    int result = *(int*)(inputBuffer + 0);
    for (int i = 0, j = sizeof(VkResult); i < pipelineCount; i++, j += VK_HANDLE_BYTE_COUNT) {
        uint64_t pipelineId = *(uint64_t*)(inputBuffer + j);
        VkObject* pipelineObject = VkObject_create(VK_OBJECT_TYPE_PIPELINE, pipelineId);
        pPipelines[i] = VkObject_toHandle(pipelineObject);
    }

    return (VkResult)result;
}

VkResult vt_call_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    VT_CALL_LOCK();

    VT_SERIALIZE_CMD(vkCreateInstance, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_INSTANCE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t instanceId;
    vt_unserialize_VkInstance((VkInstance)&instanceId, inputBuffer, &globalMemoryPool);
    VkObject* instanceObject = VkObject_create(VK_OBJECT_TYPE_INSTANCE, instanceId);
    *pInstance = VkObject_toHandle(instanceObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* instanceObject = VkObject_fromHandle(instance);

    VT_SERIALIZE_CMD(vkDestroyInstance, (VkInstance)&instanceObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_INSTANCE, outputBuffer, bufferSize);

    VkObject_free(instanceObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) {
    VT_CALL_LOCK();
    if (!pPhysicalDevices) *pPhysicalDeviceCount = 0;
    VkObject* instanceObject = VkObject_fromHandle(instance);
    
    VT_SERIALIZE_CMD(vkEnumeratePhysicalDevices, (VkInstance)&instanceObject->id, pPhysicalDeviceCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ENUMERATE_PHYSICAL_DEVICES, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkEnumeratePhysicalDevices(VK_NULL_HANDLE, pPhysicalDeviceCount, pPhysicalDevices, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

PFN_vkVoidFunction vt_call_vkGetDeviceProcAddr(VkDevice device, const char* pName) {
    return findVkDispatchFuncWithName(pName);
}

PFN_vkVoidFunction vt_call_vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    return findVkDispatchFuncWithName(pName);
}

void vt_call_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(VkPhysicalDevice, (VkPhysicalDevice)&physicalDeviceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_PROPERTIES);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkPhysicalDeviceProperties(pProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    VT_CALL_LOCK();
    if (!pQueueFamilyProperties) *pQueueFamilyPropertyCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceQueueFamilyProperties, (VkPhysicalDevice)&physicalDeviceObject->id, pQueueFamilyPropertyCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_QUEUE_FAMILY_PROPERTIES);
    VT_RECV_CHECKED();

    vt_unserialize_vkGetPhysicalDeviceQueueFamilyProperties(VK_NULL_HANDLE, pQueueFamilyPropertyCount, pQueueFamilyProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(VkPhysicalDevice, (VkPhysicalDevice)&physicalDeviceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_MEMORY_PROPERTIES);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkPhysicalDeviceMemoryProperties(pMemoryProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(VkPhysicalDevice, (VkPhysicalDevice)&physicalDeviceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_FEATURES);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkPhysicalDeviceFeatures(pFeatures, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceFormatProperties, (VkPhysicalDevice)&physicalDeviceObject->id, format, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_FORMAT_PROPERTIES);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkFormatProperties(pFormatProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceImageFormatProperties, (VkPhysicalDevice)&physicalDeviceObject->id, format, type, tiling, usage, flags, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_IMAGE_FORMAT_PROPERTIES, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_VkImageFormatProperties(pImageFormatProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);

    VT_SERIALIZE_CMD(vkCreateDevice, (VkPhysicalDevice)&physicalDeviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_DEVICE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t deviceId;
    vt_unserialize_VkDevice((VkDevice)&deviceId, inputBuffer, &globalMemoryPool);
    VkObject* deviceObject = VkObject_create(VK_OBJECT_TYPE_DEVICE, deviceId);
    *pDevice = VkObject_toHandle(deviceObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkDestroyDevice, (VkDevice)&deviceObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_DEVICE, outputBuffer, bufferSize);

    VkObject_free(deviceObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkEnumerateInstanceVersion(uint32_t* pApiVersion) {
    VT_CALL_LOCK();
    
    int bytesSent = vt_send(serverRing, REQUEST_CODE_VK_ENUMERATE_INSTANCE_VERSION, NULL, 0);
    if (bytesSent < 0) {
        VT_CALL_UNLOCK();
        return VK_ERROR_DEVICE_LOST;
    }
    
    VT_RECV_CHECKED(VT_RETURN);
    *pApiVersion = *(uint32_t*)(inputBuffer);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

VkResult vt_call_vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    VT_CALL_LOCK();
    if (!pProperties) *pPropertyCount = 0;

    VT_SERIALIZE_CMD(vkEnumerateInstanceExtensionProperties, NULL, pPropertyCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ENUMERATE_INSTANCE_EXTENSION_PROPERTIES, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkEnumerateInstanceExtensionProperties(NULL, pPropertyCount, pProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t* pPropertyCount, VkLayerProperties* pProperties) {
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

VkResult vt_call_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties) {
    VT_CALL_LOCK();
    if (!pProperties) *pPropertyCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);

    VT_SERIALIZE_CMD(vkEnumerateDeviceExtensionProperties, (VkPhysicalDevice)&physicalDeviceObject->id, NULL, pPropertyCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ENUMERATE_DEVICE_EXTENSION_PROPERTIES, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkEnumerateDeviceExtensionProperties(NULL, NULL, pPropertyCount, pProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDeviceQueue, (VkDevice)&deviceObject->id, queueFamilyIndex, queueIndex, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_QUEUE);
    VT_RECV_CHECKED();
    
    uint64_t queueId;
    vt_unserialize_VkQueue((VkQueue)&queueId, inputBuffer, &globalMemoryPool);
    VkObject* queueObject = VkObject_create(VK_OBJECT_TYPE_QUEUE, queueId);
    *pQueue = VkObject_toHandle(queueObject);
       
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    VT_CALL_LOCK();
    VkObject* queueObject = VkObject_fromHandle(queue);
    VkObject* fenceObject = VkObject_fromHandle(fence);
    
    bool shouldWait = RingBuffer_hasStatus(clientRing, RING_STATUS_WAIT);
    
    VT_SERIALIZE_CMD(vkQueueSubmit, (VkQueue)&queueObject->id, submitCount, pSubmits, (VkFence)&fenceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_QUEUE_SUBMIT, VT_RETURN);
    
    if (shouldWait) {
        VT_RECV_CHECKED(VT_RETURN);
        VT_CALL_UNLOCK();
        return (VkResult)result;
    }
    else {
        VT_CALL_UNLOCK();
        return VK_SUCCESS;        
    }
}

VkResult vt_call_vkQueueWaitIdle(VkQueue queue) {
    VT_CALL_LOCK();
    VkObject* queueObject = VkObject_fromHandle(queue);

    VT_SERIALIZE_CMD(VkQueue, (VkQueue)&queueObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_QUEUE_WAIT_IDLE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;  
}

VkResult vt_call_vkDeviceWaitIdle(VkDevice device) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(VkDevice, (VkDevice)&deviceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_DEVICE_WAIT_IDLE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;  
}

VkResult vt_call_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkAllocateMemory, (VkDevice)&deviceObject->id, pAllocateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ALLOCATE_MEMORY, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    if (result == VK_SUCCESS) {
        uint64_t memoryId;
        vt_unserialize_VkDeviceMemory((VkDeviceMemory)&memoryId, inputBuffer, &globalMemoryPool);
        VkObject* memoryObject = VkObject_create(VK_OBJECT_TYPE_DEVICE_MEMORY, memoryId);
        *pMemory = VkObject_toHandle(memoryObject);
        
        MappedMemory* mappedMemory = calloc(1, sizeof(MappedMemory));
        mappedMemory->allocationSize = pAllocateInfo->allocationSize;
        memoryObject->tag = mappedMemory;
    }
    else *pMemory = VK_NULL_HANDLE;

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* memoryObject = VkObject_fromHandle(memory);
    
    if (memoryObject->tag) MEMFREE(memoryObject->tag);

    VT_SERIALIZE_CMD(vkFreeMemory, (VkDevice)&deviceObject->id, (VkDeviceMemory)&memoryObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_FREE_MEMORY, outputBuffer, bufferSize);
    
    VkObject_free(memoryObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    VT_CALL_LOCK();    
    VkObject* memoryObject = VkObject_fromHandle(memory);

    MappedMemory* mappedMemory = memoryObject->tag;
    if (mappedMemory->data) {
        VT_CALL_UNLOCK();
        return VK_SUCCESS;
    }
    
    VT_SERIALIZE_CMD(VkDeviceMemory, (VkDeviceMemory)&memoryObject->id);    
    VT_SEND_CHECKED(REQUEST_CODE_VK_MAP_MEMORY, VT_RETURN);
    
    int fd, result, numFds;
    recv_fds(serverFd, &fd, &numFds, &result, sizeof(VkResult));
    if (numFds == 1) {
        if (size == VK_WHOLE_SIZE) size = mappedMemory->allocationSize;
        mappedMemory->size = size;
        
        void* data = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, offset);
        if (data != MAP_FAILED) {
            CLOSEFD(fd);
            mappedMemory->data = data;
            *ppData = data;
        }
        else result = VK_ERROR_MEMORY_MAP_FAILED;
    }
    else result = VK_ERROR_MEMORY_MAP_FAILED;
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    VT_CALL_LOCK();
    VkObject* memoryObject = VkObject_fromHandle(memory);
    
    if (memoryObject->tag) {
        MappedMemory* mappedMemory = memoryObject->tag;
        if (mappedMemory->data) {
            munmap(mappedMemory->data, mappedMemory->size);
            mappedMemory->data = NULL;
        }
    }

    VT_CALL_UNLOCK();
}

VkResult vt_call_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkFlushMappedMemoryRanges, (VkDevice)&deviceObject->id, memoryRangeCount, pMemoryRanges);
    VT_SEND_CHECKED(REQUEST_CODE_VK_FLUSH_MAPPED_MEMORY_RANGES, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkInvalidateMappedMemoryRanges, (VkDevice)&deviceObject->id, memoryRangeCount, pMemoryRanges);
    VT_SEND_CHECKED(REQUEST_CODE_VK_INVALIDATE_MAPPED_MEMORY_RANGES, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* memoryObject = VkObject_fromHandle(memory);
    
    VT_SERIALIZE_CMD(vkGetDeviceMemoryCommitment, (VkDevice)&deviceObject->id, (VkDeviceMemory)&memoryObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_MEMORY_COMMITMENT);
    VT_RECV_CHECKED();

    *pCommittedMemoryInBytes = *(VkDeviceSize*)(inputBuffer);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* bufferObject = VkObject_fromHandle(buffer);
    
    VT_SERIALIZE_CMD(vkGetBufferMemoryRequirements, (VkDevice)&deviceObject->id, (VkBuffer)&bufferObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_BUFFER_MEMORY_REQUIREMENTS);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkMemoryRequirements(pMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* bufferObject = VkObject_fromHandle(buffer);
    VkObject* memoryObject = VkObject_fromHandle(memory);
    
    VT_SERIALIZE_CMD(vkBindBufferMemory, (VkDevice)&deviceObject->id, (VkBuffer)&bufferObject->id, (VkDeviceMemory)&memoryObject->id, memoryOffset);
    VT_SEND_CHECKED(REQUEST_CODE_VK_BIND_BUFFER_MEMORY, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;    
}

void vt_call_vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* imageObject = VkObject_fromHandle(image);
    
    VT_SERIALIZE_CMD(vkGetImageMemoryRequirements, (VkDevice)&deviceObject->id, (VkImage)&imageObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_IMAGE_MEMORY_REQUIREMENTS);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkMemoryRequirements(pMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* imageObject = VkObject_fromHandle(image);
    VkObject* memoryObject = VkObject_fromHandle(memory);
    
    VT_SERIALIZE_CMD(vkBindImageMemory, (VkDevice)&deviceObject->id, (VkImage)&imageObject->id, (VkDeviceMemory)&memoryObject->id, memoryOffset);
    VT_SEND_CHECKED(REQUEST_CODE_VK_BIND_IMAGE_MEMORY, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    VT_CALL_LOCK();
    if (!pSparseMemoryRequirements) *pSparseMemoryRequirementCount = 0;
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* imageObject = VkObject_fromHandle(image);
    
    VT_SERIALIZE_CMD(vkGetImageSparseMemoryRequirements, (VkDevice)&deviceObject->id, (VkImage)&imageObject->id, pSparseMemoryRequirementCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_IMAGE_SPARSE_MEMORY_REQUIREMENTS);
    VT_RECV_CHECKED();

    vt_unserialize_vkGetImageSparseMemoryRequirements(VK_NULL_HANDLE, VK_NULL_HANDLE, pSparseMemoryRequirementCount, pSparseMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    VT_CALL_LOCK();
    if (!pProperties) *pPropertyCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceSparseImageFormatProperties, (VkPhysicalDevice)&physicalDeviceObject->id, format, type, samples, usage, tiling, pPropertyCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_PROPERTIES);
    VT_RECV_CHECKED();

    vt_unserialize_vkGetPhysicalDeviceSparseImageFormatProperties(VK_NULL_HANDLE, NULL, NULL, NULL, NULL, NULL, pPropertyCount, pProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    VT_CALL_LOCK();
    VkObject* queueObject = VkObject_fromHandle(queue);
    VkObject* fenceObject = VkObject_fromHandle(fence);
    
    VT_SERIALIZE_CMD(vkQueueBindSparse, (VkQueue)&queueObject->id, bindInfoCount, pBindInfo, (VkFence)&fenceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_QUEUE_BIND_SPARSE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateFence, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_FENCE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t fenceId;
    vt_unserialize_VkFence((VkFence)&fenceId, inputBuffer, &globalMemoryPool);
    VkObject* fenceObject = VkObject_create(VK_OBJECT_TYPE_FENCE, fenceId);
    *pFence = VkObject_toHandle(fenceObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* fenceObject = VkObject_fromHandle(fence);

    VT_SERIALIZE_CMD(vkDestroyFence, (VkDevice)&deviceObject->id, (VkFence)&fenceObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_FENCE, outputBuffer, bufferSize);

    VkObject_free(fenceObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);    
    
    VT_SERIALIZE_CMD(vkResetFences, (VkDevice)&deviceObject->id, fenceCount, pFences);
    VT_SEND_CHECKED(REQUEST_CODE_VK_RESET_FENCES, VT_RETURN);
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

VkResult vt_call_vkGetFenceStatus(VkDevice device, VkFence fence) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* fenceObject = VkObject_fromHandle(fence);

    VT_SERIALIZE_CMD(vkGetFenceStatus, (VkDevice)&deviceObject->id, (VkFence)&fenceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_FENCE_STATUS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;    
}

VkResult vt_call_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    VkObject* deviceObject = VkObject_fromHandle(device);
    VT_WAIT_FOR_OBJECT(REQUEST_CODE_VK_WAIT_FOR_FENCES, vkWaitForFences, (VkDevice)&deviceObject->id, fenceCount, pFences, waitAll, timeout);
}

VkResult vt_call_vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateSemaphore, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_SEMAPHORE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t semaphoreId;
    vt_unserialize_VkSemaphore((VkSemaphore)&semaphoreId, inputBuffer, &globalMemoryPool);
    VkObject* semaphoreObject = VkObject_create(VK_OBJECT_TYPE_SEMAPHORE, semaphoreId);
    *pSemaphore = VkObject_toHandle(semaphoreObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* semaphoreObject = VkObject_fromHandle(semaphore);

    VT_SERIALIZE_CMD(vkDestroySemaphore, (VkDevice)&deviceObject->id, (VkSemaphore)&semaphoreObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_SEMAPHORE, outputBuffer, bufferSize);

    VkObject_free(semaphoreObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateEvent, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_EVENT, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t eventId;
    vt_unserialize_VkEvent((VkEvent)&eventId, inputBuffer, &globalMemoryPool);
    VkObject* eventObject = VkObject_create(VK_OBJECT_TYPE_EVENT, eventId);
    *pEvent = VkObject_toHandle(eventObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* eventObject = VkObject_fromHandle(event);

    VT_SERIALIZE_CMD(vkDestroyEvent, (VkDevice)&deviceObject->id, (VkEvent)&eventObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_EVENT, outputBuffer, bufferSize);

    VkObject_free(eventObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetEventStatus(VkDevice device, VkEvent event) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* eventObject = VkObject_fromHandle(event);

    VT_SERIALIZE_CMD(vkGetEventStatus, (VkDevice)&deviceObject->id, (VkEvent)&eventObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_EVENT_STATUS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;  
}

VkResult vt_call_vkSetEvent(VkDevice device, VkEvent event) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* eventObject = VkObject_fromHandle(event);

    VT_SERIALIZE_CMD(vkSetEvent, (VkDevice)&deviceObject->id, (VkEvent)&eventObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_SET_EVENT, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result; 
}

VkResult vt_call_vkResetEvent(VkDevice device, VkEvent event) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* eventObject = VkObject_fromHandle(event);

    VT_SERIALIZE_CMD(vkResetEvent, (VkDevice)&deviceObject->id, (VkEvent)&eventObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_RESET_EVENT, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateQueryPool, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_QUERY_POOL, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t queryPoolId;
    vt_unserialize_VkQueryPool((VkQueryPool)&queryPoolId, inputBuffer, &globalMemoryPool);
    VkObject* queryPoolObject = VkObject_create(VK_OBJECT_TYPE_QUERY_POOL, queryPoolId);
    *pQueryPool = VkObject_toHandle(queryPoolObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    VT_SERIALIZE_CMD(vkDestroyQueryPool, (VkDevice)&deviceObject->id, (VkQueryPool)&queryPoolObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_QUERY_POOL, outputBuffer, bufferSize);

    VkObject_free(queryPoolObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);
    
    VT_SERIALIZE_CMD(vkGetQueryPoolResults, (VkDevice)&deviceObject->id, (VkQueryPool)&queryPoolObject->id, firstQuery, queryCount, dataSize, NULL, stride, flags);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_QUERY_POOL_RESULTS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    vt_unserialize_vkGetQueryPoolResults(VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, NULL, NULL, pData, NULL, NULL, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);
    
    VT_SERIALIZE_CMD(vkResetQueryPool, (VkDevice)&deviceObject->id, (VkQueryPool)&queryPoolObject->id, firstQuery, queryCount);
    vt_send(serverRing, REQUEST_CODE_VK_RESET_QUERY_POOL, outputBuffer, bufferSize);

    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateBuffer, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_BUFFER, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t bufferId;
    vt_unserialize_VkBuffer((VkBuffer)&bufferId, inputBuffer, &globalMemoryPool);
    VkObject* bufferObject = VkObject_create(VK_OBJECT_TYPE_BUFFER, bufferId);
    *pBuffer = VkObject_toHandle(bufferObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* bufferObject = VkObject_fromHandle(buffer);

    VT_SERIALIZE_CMD(vkDestroyBuffer, (VkDevice)&deviceObject->id, (VkBuffer)&bufferObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_BUFFER, outputBuffer, bufferSize);

    VkObject_free(bufferObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateBufferView, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_BUFFER_VIEW, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t viewId;
    vt_unserialize_VkBufferView((VkBufferView)&viewId, inputBuffer, &globalMemoryPool);
    VkObject* viewObject = VkObject_create(VK_OBJECT_TYPE_BUFFER_VIEW, viewId);
    *pView = VkObject_toHandle(viewObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* bufferViewObject = VkObject_fromHandle(bufferView);

    VT_SERIALIZE_CMD(vkDestroyBufferView, (VkDevice)&deviceObject->id, (VkBufferView)&bufferViewObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_BUFFER_VIEW, outputBuffer, bufferSize);

    VkObject_free(bufferViewObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateImage, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_IMAGE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t imageId;
    vt_unserialize_VkImage((VkImage)&imageId, inputBuffer, &globalMemoryPool);
    VkObject* imageObject = VkObject_create(VK_OBJECT_TYPE_IMAGE, imageId);
    *pImage = VkObject_toHandle(imageObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* imageObject = VkObject_fromHandle(image);

    VT_SERIALIZE_CMD(vkDestroyImage, (VkDevice)&deviceObject->id, (VkImage)&imageObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_IMAGE, outputBuffer, bufferSize);

    VkObject_free(imageObject);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* imageObject = VkObject_fromHandle(image);
    
    VT_SERIALIZE_CMD(vkGetImageSubresourceLayout, (VkDevice)&deviceObject->id, (VkImage)&imageObject->id, pSubresource, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_IMAGE_SUBRESOURCE_LAYOUT);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkSubresourceLayout(pLayout, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateImageView, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_IMAGE_VIEW, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t viewId;
    vt_unserialize_VkImageView((VkImageView)&viewId, inputBuffer, &globalMemoryPool);
    VkObject* viewObject = VkObject_create(VK_OBJECT_TYPE_IMAGE_VIEW, viewId);
    *pView = VkObject_toHandle(viewObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* imageViewObject = VkObject_fromHandle(imageView);

    VT_SERIALIZE_CMD(vkDestroyImageView, (VkDevice)&deviceObject->id, (VkImageView)&imageViewObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_IMAGE_VIEW, outputBuffer, bufferSize);

    VkObject_free(imageViewObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateShaderModule, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_SHADER_MODULE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t shaderModuleId;
    vt_unserialize_VkShaderModule((VkShaderModule)&shaderModuleId, inputBuffer, &globalMemoryPool);
    VkObject* shaderModuleObject = VkObject_create(VK_OBJECT_TYPE_SHADER_MODULE, shaderModuleId);
    *pShaderModule = VkObject_toHandle(shaderModuleObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* shaderModuleObject = VkObject_fromHandle(shaderModule);

    VT_SERIALIZE_CMD(vkDestroyShaderModule, (VkDevice)&deviceObject->id, (VkShaderModule)&shaderModuleObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_SHADER_MODULE, outputBuffer, bufferSize);

    VkObject_free(shaderModuleObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreatePipelineCache, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_PIPELINE_CACHE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t pipelineCacheId;
    vt_unserialize_VkPipelineCache((VkPipelineCache)&pipelineCacheId, inputBuffer, &globalMemoryPool);
    VkObject* pipelineCacheObject = VkObject_create(VK_OBJECT_TYPE_PIPELINE_CACHE, pipelineCacheId);
    *pPipelineCache = VkObject_toHandle(pipelineCacheObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* pipelineCacheObject = VkObject_fromHandle(pipelineCache);

    VT_SERIALIZE_CMD(vkDestroyPipelineCache, (VkDevice)&deviceObject->id, (VkPipelineCache)&pipelineCacheObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_PIPELINE_CACHE, outputBuffer, bufferSize);

    VkObject_free(pipelineCacheObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    VT_CALL_LOCK();
    if (!pData) *pDataSize = 0;
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* pipelineCacheObject = VkObject_fromHandle(pipelineCache);
    
    VT_SERIALIZE_CMD(vkGetPipelineCacheData, (VkDevice)&deviceObject->id, (VkPipelineCache)&pipelineCacheObject->id, pDataSize, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PIPELINE_CACHE_DATA, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    vt_unserialize_vkGetPipelineCacheData(VK_NULL_HANDLE, VK_NULL_HANDLE, pDataSize, pData, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* dstCacheObject = VkObject_fromHandle(dstCache);
    
    VT_SERIALIZE_CMD(vkMergePipelineCaches, (VkDevice)&deviceObject->id, (VkPipelineCache)&dstCacheObject->id, srcCacheCount, pSrcCaches);
    VT_SEND_CHECKED(REQUEST_CODE_VK_MERGE_PIPELINE_CACHES, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* pipelineCacheObject = VkObject_fromHandle(pipelineCache);

    VT_SERIALIZE_CMD(vkCreateGraphicsPipelines, (VkDevice)&deviceObject->id, (VkPipelineCache)&pipelineCacheObject->id, createInfoCount, pCreateInfos, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_GRAPHICS_PIPELINES, VT_RETURN);

    return waitForPipelineCreation(createInfoCount, pPipelines);
}

VkResult vt_call_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* pipelineCacheObject = VkObject_fromHandle(pipelineCache);

    VT_SERIALIZE_CMD(vkCreateComputePipelines, (VkDevice)&deviceObject->id, (VkPipelineCache)&pipelineCacheObject->id, createInfoCount, pCreateInfos, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_COMPUTE_PIPELINES, VT_RETURN);

    return waitForPipelineCreation(createInfoCount, pPipelines);
}

void vt_call_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* pipelineObject = VkObject_fromHandle(pipeline);

    VT_SERIALIZE_CMD(vkDestroyPipeline, (VkDevice)&deviceObject->id, (VkPipeline)&pipelineObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_PIPELINE, outputBuffer, bufferSize);

    VkObject_free(pipelineObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreatePipelineLayout, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_PIPELINE_LAYOUT, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t pipelineLayoutId;
    vt_unserialize_VkPipelineLayout((VkPipelineLayout)&pipelineLayoutId, inputBuffer, &globalMemoryPool);
    VkObject* pipelineLayoutObject = VkObject_create(VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipelineLayoutId);
    *pPipelineLayout = VkObject_toHandle(pipelineLayoutObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* pipelineLayoutObject = VkObject_fromHandle(pipelineLayout);

    VT_SERIALIZE_CMD(vkDestroyPipelineLayout, (VkDevice)&deviceObject->id, (VkPipelineLayout)&pipelineLayoutObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_PIPELINE_LAYOUT, outputBuffer, bufferSize);

    VkObject_free(pipelineLayoutObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateSampler, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_SAMPLER, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t samplerId;
    vt_unserialize_VkSampler((VkSampler)&samplerId, inputBuffer, &globalMemoryPool);
    VkObject* samplerObject = VkObject_create(VK_OBJECT_TYPE_SAMPLER, samplerId);
    *pSampler = VkObject_toHandle(samplerObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* samplerObject = VkObject_fromHandle(sampler);

    VT_SERIALIZE_CMD(vkDestroySampler, (VkDevice)&deviceObject->id, (VkSampler)&samplerObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_SAMPLER, outputBuffer, bufferSize);

    VkObject_free(samplerObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateDescriptorSetLayout, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_DESCRIPTOR_SET_LAYOUT, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t setLayoutId;
    vt_unserialize_VkDescriptorSetLayout((VkDescriptorSetLayout)&setLayoutId, inputBuffer, &globalMemoryPool);
    VkObject* setLayoutObject = VkObject_create(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, setLayoutId);
    *pSetLayout = VkObject_toHandle(setLayoutObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* descriptorSetLayoutObject = VkObject_fromHandle(descriptorSetLayout);

    VT_SERIALIZE_CMD(vkDestroyDescriptorSetLayout, (VkDevice)&deviceObject->id, (VkDescriptorSetLayout)&descriptorSetLayoutObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_DESCRIPTOR_SET_LAYOUT, outputBuffer, bufferSize);

    VkObject_free(descriptorSetLayoutObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateDescriptorPool, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_DESCRIPTOR_POOL, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t descriptorPoolId;
    vt_unserialize_VkDescriptorPool((VkDescriptorPool)&descriptorPoolId, inputBuffer, &globalMemoryPool);
    VkObject* descriptorPoolObject = VkObject_create(VK_OBJECT_TYPE_DESCRIPTOR_POOL, descriptorPoolId);
    *pDescriptorPool = VkObject_toHandle(descriptorPoolObject);
    
    bool canFreeObjects = (pCreateInfo->flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    descriptorPoolObject->tag = VkObjectPool_create(VK_OBJECT_TYPE_DESCRIPTOR_SET, pCreateInfo->maxSets, canFreeObjects);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* descriptorPoolObject = VkObject_fromHandle(descriptorPool);

    VT_SERIALIZE_CMD(vkDestroyDescriptorPool, (VkDevice)&deviceObject->id, (VkDescriptorPool)&descriptorPoolObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_DESCRIPTOR_POOL, outputBuffer, bufferSize);
    
    VkObjectPool_destroy((VkObjectPool*)descriptorPoolObject->tag);
    VkObject_free(descriptorPoolObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* descriptorPoolObject = VkObject_fromHandle(descriptorPool);
    
    VT_SERIALIZE_CMD(vkResetDescriptorPool, (VkDevice)&deviceObject->id, (VkDescriptorPool)&descriptorPoolObject->id, flags);
    VT_SEND_CHECKED(REQUEST_CODE_VK_RESET_DESCRIPTOR_POOL, VT_RETURN);
    
    VkObjectPool_reset((VkObjectPool*)descriptorPoolObject->tag);
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

VkResult vt_call_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* descriptorPoolObject = VkObject_fromHandle(pAllocateInfo->descriptorPool);
    
    VT_SERIALIZE_CMD(vkAllocateDescriptorSets, (VkDevice)&deviceObject->id, pAllocateInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ALLOCATE_DESCRIPTOR_SETS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    if (result != VK_SUCCESS) {
        VT_CALL_UNLOCK();
        return (VkResult)result;
    }
    
    VkObjectPool* objectPool = descriptorPoolObject->tag;
    for (int i = 0, j = 0; i < pAllocateInfo->descriptorSetCount; i++, j += VK_HANDLE_BYTE_COUNT) {
        uint64_t descriptorSetId;
        vt_unserialize_VkDescriptorSet((VkDescriptorSet)&descriptorSetId, inputBuffer + j, NULL);
        
        VkObject* descriptorSetObject = VkObjectPool_get(objectPool);
        if (!descriptorSetObject) {
            VT_CALL_UNLOCK();
            return VK_ERROR_OUT_OF_POOL_MEMORY;
        }        
        
        descriptorSetObject->id = descriptorSetId;
        pDescriptorSets[i] = VkObject_toHandle(descriptorSetObject);
    }    
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* descriptorPoolObject = VkObject_fromHandle(descriptorPool);
    
    VT_SERIALIZE_CMD(vkFreeDescriptorSets, (VkDevice)&deviceObject->id, (VkDescriptorPool)&descriptorPoolObject->id, descriptorSetCount, pDescriptorSets);
    VT_SEND_CHECKED(REQUEST_CODE_VK_FREE_DESCRIPTOR_SETS, VT_RETURN);
    
    VkObjectPool* objectPool = descriptorPoolObject->tag;
    for (int i = 0; i < descriptorSetCount; i++) {
        VkObject* descriptorSetObject = VkObject_fromHandle(pDescriptorSets[i]);
        VkObjectPool_freeObject(objectPool, descriptorSetObject);
    }
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

void vt_call_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkUpdateDescriptorSets, (VkDevice)&deviceObject->id, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    vt_send(serverRing, REQUEST_CODE_VK_UPDATE_DESCRIPTOR_SETS, outputBuffer, bufferSize);
    
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateFramebuffer, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_FRAMEBUFFER, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t framebufferId;
    vt_unserialize_VkFramebuffer((VkFramebuffer)&framebufferId, inputBuffer, &globalMemoryPool);
    VkObject* framebufferObject = VkObject_create(VK_OBJECT_TYPE_FRAMEBUFFER, framebufferId);
    *pFramebuffer = VkObject_toHandle(framebufferObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* framebufferObject = VkObject_fromHandle(framebuffer);

    VT_SERIALIZE_CMD(vkDestroyFramebuffer, (VkDevice)&deviceObject->id, (VkFramebuffer)&framebufferObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_FRAMEBUFFER, outputBuffer, bufferSize);

    VkObject_free(framebufferObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateRenderPass, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_RENDER_PASS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t renderPassId;
    vt_unserialize_VkRenderPass((VkRenderPass)&renderPassId, inputBuffer, &globalMemoryPool);
    VkObject* renderPassObject = VkObject_create(VK_OBJECT_TYPE_RENDER_PASS, renderPassId);
    *pRenderPass = VkObject_toHandle(renderPassObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* renderPassObject = VkObject_fromHandle(renderPass);

    VT_SERIALIZE_CMD(vkDestroyRenderPass, (VkDevice)&deviceObject->id, (VkRenderPass)&renderPassObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_RENDER_PASS, outputBuffer, bufferSize);

    VkObject_free(renderPassObject);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* renderPassObject = VkObject_fromHandle(renderPass);
    
    VT_SERIALIZE_CMD(vkGetRenderAreaGranularity, (VkDevice)&deviceObject->id, (VkRenderPass)&renderPassObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_RENDER_AREA_GRANULARITY);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkExtent2D(pGranularity, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateCommandPool, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_COMMAND_POOL, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t commandPoolId;
    vt_unserialize_VkCommandPool((VkCommandPool)&commandPoolId, inputBuffer, &globalMemoryPool);
    VkObject* commandPoolObject = VkObject_create(VK_OBJECT_TYPE_COMMAND_POOL, commandPoolId);
    *pCommandPool = VkObject_toHandle(commandPoolObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* commandPoolObject = VkObject_fromHandle(commandPool);

    VT_SERIALIZE_CMD(vkDestroyCommandPool, (VkDevice)&deviceObject->id, (VkCommandPool)&commandPoolObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_COMMAND_POOL, outputBuffer, bufferSize);

    VkObject_free(commandPoolObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* commandPoolObject = VkObject_fromHandle(commandPool);
    
    VT_SERIALIZE_CMD(vkResetCommandPool, (VkDevice)&deviceObject->id, (VkCommandPool)&commandPoolObject->id, flags);
    VT_SEND_CHECKED(REQUEST_CODE_VK_RESET_COMMAND_POOL, VT_RETURN);
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

VkResult vt_call_vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkAllocateCommandBuffers, (VkDevice)&deviceObject->id, pAllocateInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ALLOCATE_COMMAND_BUFFERS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VkCommandBufferAllocateInfo allocateInfo = {0};
    vt_unserialize_vkAllocateCommandBuffers(VK_NULL_HANDLE, &allocateInfo, pCommandBuffers, inputBuffer, &globalMemoryPool);
    
    for (int i = 0; i < pAllocateInfo->commandBufferCount; i++) {
        VkObject* commandBufferObject = VkObject_fromHandle(pCommandBuffers[i]);
        CommandBatch* batch = calloc(1, sizeof(CommandBatch));
        ENSURE_ARRAY_CAPACITY(VK_HANDLE_BYTE_COUNT, batch->capacity, batch->buffer, 1);
        commandBufferObject->tag = batch;
    }
    
    VT_CALL_UNLOCK();
    return (VkResult)result;    
}

void vt_call_vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* commandPoolObject = VkObject_fromHandle(commandPool);
    
    for (int i = 0; i < commandBufferCount; i++) {
        VkObject* commandBufferObject = VkObject_fromHandle(pCommandBuffers[i]);
        CommandBatch* batch = commandBufferObject->tag;
        batch->size = 0;
        batch->capacity = 0;
        
        MEMFREE(batch->buffer);
        MEMFREE(commandBufferObject->tag);
    }
    
    VT_SERIALIZE_CMD(vkFreeCommandBuffers, (VkDevice)&deviceObject->id, (VkCommandPool)&commandPoolObject->id, commandBufferCount, pCommandBuffers);
    vt_send(serverRing, REQUEST_CODE_VK_FREE_COMMAND_BUFFERS, outputBuffer, bufferSize);  
    
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    VT_CALL_LOCK();
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    
    CommandBatch* batch = commandBufferObject->tag;
    *(uint64_t*)(batch->buffer) = commandBufferObject->id;
    batch->size = VK_HANDLE_BYTE_COUNT;
    
    VT_SERIALIZE_CMD(vkBeginCommandBuffer, (VkCommandBuffer)&commandBufferObject->id, pBeginInfo);
    VT_SEND_CHECKED(REQUEST_CODE_VK_BEGIN_COMMAND_BUFFER, VT_RETURN);
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;    
}

VkResult vt_call_vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    VT_CALL_LOCK();
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    CommandBatch* batch = commandBufferObject->tag;
    
    if (batch->size == 0) {
        VT_CALL_UNLOCK();
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vt_send(serverRing, REQUEST_CODE_VK_END_COMMAND_BUFFER, batch->buffer, batch->size);
    
    batch->size = 0;
    VT_CALL_UNLOCK();
    return VK_SUCCESS;   
}

VkResult vt_call_vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    VT_CALL_LOCK();
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    CommandBatch* batch = commandBufferObject->tag;
    batch->size = 0;
    
    VT_SERIALIZE_CMD(vkResetCommandBuffer, (VkCommandBuffer)&commandBufferObject->id, flags);    
    VT_SEND_CHECKED(REQUEST_CODE_VK_RESET_COMMAND_BUFFER, VT_RETURN);
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;     
}

void vt_call_vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* pipelineObject = VkObject_fromHandle(pipeline);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBindPipeline, REQUEST_CODE_VK_CMD_BIND_PIPELINE, batch, (VkCommandBuffer)&commandBufferObject->id, pipelineBindPoint, (VkPipeline)&pipelineObject->id);
}

void vt_call_vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetViewport, REQUEST_CODE_VK_CMD_SET_VIEWPORT, batch, (VkCommandBuffer)&commandBufferObject->id, firstViewport, viewportCount, pViewports);
}

void vt_call_vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetScissor, REQUEST_CODE_VK_CMD_SET_SCISSOR, batch, (VkCommandBuffer)&commandBufferObject->id, firstScissor, scissorCount, pScissors);
}

void vt_call_vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetLineWidth, REQUEST_CODE_VK_CMD_SET_LINE_WIDTH, batch, (VkCommandBuffer)&commandBufferObject->id, lineWidth);
}

void vt_call_vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthBias, REQUEST_CODE_VK_CMD_SET_DEPTH_BIAS, batch, (VkCommandBuffer)&commandBufferObject->id, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

void vt_call_vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4]) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetBlendConstants, REQUEST_CODE_VK_CMD_SET_BLEND_CONSTANTS, batch, (VkCommandBuffer)&commandBufferObject->id, blendConstants);
}

void vt_call_vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthBounds, REQUEST_CODE_VK_CMD_SET_DEPTH_BOUNDS, batch, (VkCommandBuffer)&commandBufferObject->id, minDepthBounds, maxDepthBounds);
}

void vt_call_vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetStencilCompareMask, REQUEST_CODE_VK_CMD_SET_STENCIL_COMPARE_MASK, batch, (VkCommandBuffer)&commandBufferObject->id, faceMask, compareMask);
}

void vt_call_vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetStencilWriteMask, REQUEST_CODE_VK_CMD_SET_STENCIL_WRITE_MASK, batch, (VkCommandBuffer)&commandBufferObject->id, faceMask, writeMask);
}

void vt_call_vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetStencilReference, REQUEST_CODE_VK_CMD_SET_STENCIL_REFERENCE, batch, (VkCommandBuffer)&commandBufferObject->id, faceMask, reference);
}

void vt_call_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* layoutObject = VkObject_fromHandle(layout);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBindDescriptorSets, REQUEST_CODE_VK_CMD_BIND_DESCRIPTOR_SETS, batch, (VkCommandBuffer)&commandBufferObject->id, pipelineBindPoint, (VkPipelineLayout)&layoutObject->id, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

void vt_call_vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* bufferObject = VkObject_fromHandle(buffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBindIndexBuffer, REQUEST_CODE_VK_CMD_BIND_INDEX_BUFFER, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&bufferObject->id, offset, indexType);
}

void vt_call_vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBindVertexBuffers, REQUEST_CODE_VK_CMD_BIND_VERTEX_BUFFERS, batch, (VkCommandBuffer)&commandBufferObject->id, firstBinding, bindingCount, pBuffers, pOffsets);
}

void vt_call_vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDraw, REQUEST_CODE_VK_CMD_DRAW, batch, (VkCommandBuffer)&commandBufferObject->id, vertexCount, instanceCount, firstVertex, firstInstance);
}

void vt_call_vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDrawIndexed, REQUEST_CODE_VK_CMD_DRAW_INDEXED, batch, (VkCommandBuffer)&commandBufferObject->id, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void vt_call_vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* bufferObject = VkObject_fromHandle(buffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDrawIndirect, REQUEST_CODE_VK_CMD_DRAW_INDIRECT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&bufferObject->id, offset, drawCount, stride);
}

void vt_call_vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* bufferObject = VkObject_fromHandle(buffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDrawIndexedIndirect, REQUEST_CODE_VK_CMD_DRAW_INDEXED_INDIRECT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&bufferObject->id, offset, drawCount, stride);
}

void vt_call_vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDispatch, REQUEST_CODE_VK_CMD_DISPATCH, batch, (VkCommandBuffer)&commandBufferObject->id, groupCountX, groupCountY, groupCountZ);
}

void vt_call_vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* bufferObject = VkObject_fromHandle(buffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDispatchIndirect, REQUEST_CODE_VK_CMD_DISPATCH_INDIRECT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&bufferObject->id, offset);
}

void vt_call_vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* srcBufferObject = VkObject_fromHandle(srcBuffer);
    VkObject* dstBufferObject = VkObject_fromHandle(dstBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyBuffer, REQUEST_CODE_VK_CMD_COPY_BUFFER, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&srcBufferObject->id, (VkBuffer)&dstBufferObject->id, regionCount, pRegions);
}

void vt_call_vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* srcImageObject = VkObject_fromHandle(srcImage);
    VkObject* dstImageObject = VkObject_fromHandle(dstImage);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyImage, REQUEST_CODE_VK_CMD_COPY_IMAGE, batch, (VkCommandBuffer)&commandBufferObject->id, (VkImage)&srcImageObject->id, srcImageLayout, (VkImage)&dstImageObject->id, dstImageLayout, regionCount, pRegions);
}

void vt_call_vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* srcImageObject = VkObject_fromHandle(srcImage);
    VkObject* dstImageObject = VkObject_fromHandle(dstImage);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBlitImage, REQUEST_CODE_VK_CMD_BLIT_IMAGE, batch, (VkCommandBuffer)&commandBufferObject->id, (VkImage)&srcImageObject->id, srcImageLayout, (VkImage)&dstImageObject->id, dstImageLayout, regionCount, pRegions, filter);
}

void vt_call_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* srcBufferObject = VkObject_fromHandle(srcBuffer);
    VkObject* dstImageObject = VkObject_fromHandle(dstImage);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyBufferToImage, REQUEST_CODE_VK_CMD_COPY_BUFFER_TO_IMAGE, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&srcBufferObject->id, (VkImage)&dstImageObject->id, dstImageLayout, regionCount, pRegions);
}

void vt_call_vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* srcImageObject = VkObject_fromHandle(srcImage);
    VkObject* dstBufferObject = VkObject_fromHandle(dstBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyImageToBuffer, REQUEST_CODE_VK_CMD_COPY_IMAGE_TO_BUFFER, batch, (VkCommandBuffer)&commandBufferObject->id, (VkImage)&srcImageObject->id, srcImageLayout, (VkBuffer)&dstBufferObject->id, regionCount, pRegions);
}

void vt_call_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* dstBufferObject = VkObject_fromHandle(dstBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdUpdateBuffer, REQUEST_CODE_VK_CMD_UPDATE_BUFFER, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&dstBufferObject->id, dstOffset, dataSize, pData);
}

void vt_call_vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* dstBufferObject = VkObject_fromHandle(dstBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdFillBuffer, REQUEST_CODE_VK_CMD_FILL_BUFFER, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&dstBufferObject->id, dstOffset, size, data);
}

void vt_call_vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* imageObject = VkObject_fromHandle(image);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdClearColorImage, REQUEST_CODE_VK_CMD_CLEAR_COLOR_IMAGE, batch, (VkCommandBuffer)&commandBufferObject->id, (VkImage)&imageObject->id, imageLayout, pColor, rangeCount, pRanges);
}

void vt_call_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* imageObject = VkObject_fromHandle(image);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdClearDepthStencilImage, REQUEST_CODE_VK_CMD_CLEAR_DEPTH_STENCIL_IMAGE, batch, (VkCommandBuffer)&commandBufferObject->id, (VkImage)&imageObject->id, imageLayout, pDepthStencil, rangeCount, pRanges);
}

void vt_call_vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdClearAttachments, REQUEST_CODE_VK_CMD_CLEAR_ATTACHMENTS, batch, (VkCommandBuffer)&commandBufferObject->id, attachmentCount, pAttachments, rectCount, pRects);
}

void vt_call_vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* srcImageObject = VkObject_fromHandle(srcImage);
    VkObject* dstImageObject = VkObject_fromHandle(dstImage);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdResolveImage, REQUEST_CODE_VK_CMD_RESOLVE_IMAGE, batch, (VkCommandBuffer)&commandBufferObject->id, (VkImage)&srcImageObject->id, srcImageLayout, (VkImage)&dstImageObject->id, dstImageLayout, regionCount, pRegions);
}

void vt_call_vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* eventObject = VkObject_fromHandle(event);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetEvent, REQUEST_CODE_VK_CMD_SET_EVENT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkEvent)&eventObject->id, stageMask);
}

void vt_call_vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* eventObject = VkObject_fromHandle(event);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdResetEvent, REQUEST_CODE_VK_CMD_RESET_EVENT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkEvent)&eventObject->id, stageMask);
}

void vt_call_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdWaitEvents, REQUEST_CODE_VK_CMD_WAIT_EVENTS, batch, (VkCommandBuffer)&commandBufferObject->id, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void vt_call_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdPipelineBarrier, REQUEST_CODE_VK_CMD_PIPELINE_BARRIER, batch, (VkCommandBuffer)&commandBufferObject->id, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void vt_call_vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBeginQuery, REQUEST_CODE_VK_CMD_BEGIN_QUERY, batch, (VkCommandBuffer)&commandBufferObject->id, (VkQueryPool)&queryPoolObject->id, query, flags);
}

void vt_call_vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdEndQuery, REQUEST_CODE_VK_CMD_END_QUERY, batch, (VkCommandBuffer)&commandBufferObject->id, (VkQueryPool)&queryPoolObject->id, query);
}

void vt_call_vkCmdBeginConditionalRenderingEXT(VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBeginConditionalRenderingEXT, REQUEST_CODE_VK_CMD_BEGIN_CONDITIONAL_RENDERING_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, pConditionalRenderingBegin);
}

void vt_call_vkCmdEndConditionalRenderingEXT(VkCommandBuffer commandBuffer) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(VkCommandBuffer, REQUEST_CODE_VK_CMD_END_CONDITIONAL_RENDERING_EXT, batch, (VkCommandBuffer)&commandBufferObject->id);
}

void vt_call_vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdResetQueryPool, REQUEST_CODE_VK_CMD_RESET_QUERY_POOL, batch, (VkCommandBuffer)&commandBufferObject->id, (VkQueryPool)&queryPoolObject->id, firstQuery, queryCount);
}

void vt_call_vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdWriteTimestamp, REQUEST_CODE_VK_CMD_WRITE_TIMESTAMP, batch, (VkCommandBuffer)&commandBufferObject->id, pipelineStage, (VkQueryPool)&queryPoolObject->id, query);
}

void vt_call_vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);
    VkObject* dstBufferObject = VkObject_fromHandle(dstBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyQueryPoolResults, REQUEST_CODE_VK_CMD_COPY_QUERY_POOL_RESULTS, batch, (VkCommandBuffer)&commandBufferObject->id, (VkQueryPool)&queryPoolObject->id, firstQuery, queryCount, (VkBuffer)&dstBufferObject->id, dstOffset, stride, flags);
}

void vt_call_vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* layoutObject = VkObject_fromHandle(layout);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdPushConstants, REQUEST_CODE_VK_CMD_PUSH_CONSTANTS, batch, (VkCommandBuffer)&commandBufferObject->id, (VkPipelineLayout)&layoutObject->id, stageFlags, offset, size, pValues);
}

void vt_call_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBeginRenderPass, REQUEST_CODE_VK_CMD_BEGIN_RENDER_PASS, batch, (VkCommandBuffer)&commandBufferObject->id, pRenderPassBegin, contents);
}

void vt_call_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdNextSubpass, REQUEST_CODE_VK_CMD_NEXT_SUBPASS, batch, (VkCommandBuffer)&commandBufferObject->id, contents);
}

void vt_call_vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(VkCommandBuffer, REQUEST_CODE_VK_CMD_END_RENDER_PASS, batch, (VkCommandBuffer)&commandBufferObject->id);
}

void vt_call_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdExecuteCommands, REQUEST_CODE_VK_CMD_EXECUTE_COMMANDS, batch, (VkCommandBuffer)&commandBufferObject->id, commandBufferCount, pCommandBuffers);
}

void vt_call_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) {
    VkObject* surfaceObject = VkObject_fromHandle(surface);
    VkObject_free(surfaceObject);
}

VkResult vt_call_vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported) {
    *pSupported = VK_TRUE;
    return VK_SUCCESS;
}

VkResult vt_call_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    VkObject* surfaceObject = VkObject_fromHandle(surface);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, (VkPhysicalDevice)&physicalDeviceObject->id, (VkSurfaceKHR)&surfaceObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_SURFACE_CAPABILITIES_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    vt_unserialize_VkSurfaceCapabilitiesKHR(pSurfaceCapabilities, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pSurfaceFormatCount, VkSurfaceFormatKHR* pSurfaceFormats) {
    VT_CALL_LOCK();
    if (!pSurfaceFormats) *pSurfaceFormatCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);

    VT_SERIALIZE_CMD(vkGetPhysicalDeviceSurfaceFormatsKHR, (VkPhysicalDevice)&physicalDeviceObject->id, NULL, pSurfaceFormatCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_SURFACE_FORMATS_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkGetPhysicalDeviceSurfaceFormatsKHR(VK_NULL_HANDLE, NULL, pSurfaceFormatCount, pSurfaceFormats, inputBuffer, &globalMemoryPool);    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pPresentModeCount, VkPresentModeKHR* pPresentModes) {
    VT_CALL_LOCK();
    if (!pPresentModes) *pPresentModeCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);

    VT_SERIALIZE_CMD(vkGetPhysicalDeviceSurfacePresentModesKHR, (VkPhysicalDevice)&physicalDeviceObject->id, NULL, pPresentModeCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_SURFACE_PRESENT_MODES_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkGetPhysicalDeviceSurfacePresentModesKHR(VK_NULL_HANDLE, NULL, pPresentModeCount, pPresentModes, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VkSwapchainCreateInfoKHR createInfo = {0};
    memcpy(&createInfo, pCreateInfo, sizeof(VkSwapchainCreateInfoKHR));
    VkObject* surfaceObject = VkObject_fromHandle(pCreateInfo->surface);
    createInfo.surface = (VkSurfaceKHR)&surfaceObject->id;
    
    VT_SERIALIZE_CMD(vkCreateSwapchainKHR, (VkDevice)&deviceObject->id, &createInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_SWAPCHAIN_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    uint64_t swapchainId;
    vt_unserialize_VkSwapchainKHR((VkSwapchainKHR)&swapchainId, inputBuffer, &globalMemoryPool);
    VkObject* swapchainObject = VkObject_create(VK_OBJECT_TYPE_SWAPCHAIN_KHR, swapchainId);
    *pSwapchain = VkObject_toHandle(swapchainObject);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;     
}

void vt_call_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* swapchainObject = VkObject_fromHandle(swapchain);

    VT_SERIALIZE_CMD(vkDestroySwapchainKHR, (VkDevice)&deviceObject->id, (VkSwapchainKHR)&swapchainObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_SWAPCHAIN_KHR, outputBuffer, bufferSize);

    VkObject_free(swapchainObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    VT_CALL_LOCK();
    if (!pSwapchainImages) *pSwapchainImageCount = 0;
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* swapchainObject = VkObject_fromHandle(swapchain);

    VT_SERIALIZE_CMD(vkGetSwapchainImagesKHR, (VkDevice)&deviceObject->id, (VkSwapchainKHR)&swapchainObject->id, pSwapchainImageCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_SWAPCHAIN_IMAGES_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkGetSwapchainImagesKHR(VK_NULL_HANDLE, VK_NULL_HANDLE, pSwapchainImageCount, pSwapchainImages, inputBuffer, &globalMemoryPool);    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* swapchainObject = VkObject_fromHandle(swapchain);
    VkObject* semaphoreObject = VkObject_fromHandle(semaphore);
    VkObject* fenceObject = VkObject_fromHandle(fence);
 
    VT_SERIALIZE_CMD(vkAcquireNextImageKHR, (VkDevice)&deviceObject->id, (VkSwapchainKHR)&swapchainObject->id, timeout, (VkSemaphore)&semaphoreObject->id, (VkFence)&fenceObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ACQUIRE_NEXT_IMAGE_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    if (result >= 0 && result <= 10) {
        *pImageIndex = result;
        result = VK_SUCCESS;
    }

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    VT_CALL_LOCK();
    
    VT_SERIALIZE_CMD(VkPresentInfoKHR, pPresentInfo);
    VT_SEND_CHECKED(REQUEST_CODE_VK_QUEUE_PRESENT_KHR, VT_RETURN);
    
    if (pPresentInfo->pResults) {
        for (int i = 0; i < pPresentInfo->swapchainCount; i++) {
            pPresentInfo->pResults[i] = VK_SUCCESS;
        }
    }
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

VkResult vt_call_vkCreateXlibSurfaceKHR(VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
    VkObject* surfaceObject = VkObject_create(VK_OBJECT_TYPE_SURFACE_KHR, (uint64_t)pCreateInfo->window);
    *pSurface = VkObject_toHandle(surfaceObject);
    return VK_SUCCESS;
}

VkBool32 vt_call_vkGetPhysicalDeviceXlibPresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, Display* dpy, VisualID visualID) {
    return VK_TRUE;
}

void vt_call_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceFeatures2, (VkPhysicalDevice)&physicalDeviceObject->id, pFeatures);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_FEATURES2);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkPhysicalDeviceFeatures2(pFeatures, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceProperties2, (VkPhysicalDevice)&physicalDeviceObject->id, pProperties);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_PROPERTIES2);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkPhysicalDeviceProperties2(pProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceFormatProperties2, (VkPhysicalDevice)&physicalDeviceObject->id, format, pFormatProperties);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_FORMAT_PROPERTIES2);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkFormatProperties2(pFormatProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceImageFormatProperties2, (VkPhysicalDevice)&physicalDeviceObject->id, pImageFormatInfo, pImageFormatProperties);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_IMAGE_FORMAT_PROPERTIES2, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_VkImageFormatProperties2(pImageFormatProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    VT_CALL_LOCK();
    if (!pQueueFamilyProperties) *pQueueFamilyPropertyCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceQueueFamilyProperties2, (VkPhysicalDevice)&physicalDeviceObject->id, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_QUEUE_FAMILY_PROPERTIES2);
    VT_RECV_CHECKED();

    vt_unserialize_vkGetPhysicalDeviceQueueFamilyProperties2(VK_NULL_HANDLE, pQueueFamilyPropertyCount, pQueueFamilyProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceMemoryProperties2, (VkPhysicalDevice)&physicalDeviceObject->id, pMemoryProperties);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_MEMORY_PROPERTIES2);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkPhysicalDeviceMemoryProperties2(pMemoryProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    VT_CALL_LOCK();
    if (!pProperties) *pPropertyCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceSparseImageFormatProperties2, (VkPhysicalDevice)&physicalDeviceObject->id, pFormatInfo, pPropertyCount, pProperties);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_PROPERTIES2);
    VT_RECV_CHECKED();

    vt_unserialize_vkGetPhysicalDeviceSparseImageFormatProperties2(VK_NULL_HANDLE, NULL, pPropertyCount, pProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* layoutObject = VkObject_fromHandle(layout);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdPushDescriptorSetKHR, REQUEST_CODE_VK_CMD_PUSH_DESCRIPTOR_SET_KHR, batch, (VkCommandBuffer)&commandBufferObject->id, pipelineBindPoint, (VkPipelineLayout)&layoutObject->id, set, descriptorWriteCount, pDescriptorWrites);
}

void vt_call_vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* commandPoolObject = VkObject_fromHandle(commandPool);
    
    VT_SERIALIZE_CMD(vkTrimCommandPool, (VkDevice)&deviceObject->id, (VkCommandPool)&commandPoolObject->id, flags);
    VT_SEND_CHECKED(REQUEST_CODE_VK_TRIM_COMMAND_POOL);
    
    VT_CALL_UNLOCK();
}

void vt_call_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceExternalBufferProperties, (VkPhysicalDevice)&physicalDeviceObject->id, pExternalBufferInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_EXTERNAL_BUFFER_PROPERTIES);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkExternalBufferProperties(pExternalBufferProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetMemoryFdKHR(VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) {
    VT_CALL_LOCK();
    VkObject* memoryObject = VkObject_fromHandle(pGetFdInfo->memory);
    
    VT_SERIALIZE_CMD(VkDeviceMemory, (VkDeviceMemory)&memoryObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_MEMORY_FD_KHR, VT_RETURN);

    *pFd = -1;
    int result, numFds;
    recv_fds(serverFd, pFd, &numFds, &result, sizeof(VkResult));
       
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkGetMemoryFdPropertiesKHR(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

void vt_call_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceExternalSemaphoreProperties, (VkPhysicalDevice)&physicalDeviceObject->id, pExternalSemaphoreInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_PROPERTIES);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkExternalSemaphoreProperties(pExternalSemaphoreProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetSemaphoreFdKHR(VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetSemaphoreFdKHR, (VkDevice)&deviceObject->id, pGetFdInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_SEMAPHORE_FD_KHR, VT_RETURN);

    *pFd = -1;
    int result, numFds;
    recv_fds(serverFd, pFd, &numFds, &result, sizeof(VkResult));
       
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkImportSemaphoreFdKHR(VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

void vt_call_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceExternalFenceProperties, (VkPhysicalDevice)&physicalDeviceObject->id, pExternalFenceInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_EXTERNAL_FENCE_PROPERTIES);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkExternalFenceProperties(pExternalFenceProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetFenceFdKHR(VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetFenceFdKHR, (VkDevice)&deviceObject->id, pGetFdInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_FENCE_FD_KHR, VT_RETURN);

    *pFd = -1;
    int result, numFds;
    recv_fds(serverFd, pFd, &numFds, &result, sizeof(VkResult));
       
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkImportFenceFdKHR(VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) {
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

VkResult vt_call_vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t* pPhysicalDeviceGroupCount, VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) {
    VT_CALL_LOCK();
    if (!pPhysicalDeviceGroupProperties) *pPhysicalDeviceGroupCount = 0;
    VkObject* instanceObject = VkObject_fromHandle(instance);

    VT_SERIALIZE_CMD(vkEnumeratePhysicalDeviceGroups, (VkInstance)&instanceObject->id, pPhysicalDeviceGroupCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ENUMERATE_PHYSICAL_DEVICE_GROUPS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkEnumeratePhysicalDeviceGroups(VK_NULL_HANDLE, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDeviceGroupPeerMemoryFeatures, (VkDevice)&deviceObject->id, heapIndex, localDeviceIndex, remoteDeviceIndex, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_GROUP_PEER_MEMORY_FEATURES);
    VT_RECV_CHECKED();
    
    *pPeerMemoryFeatures = *(uint32_t*)(inputBuffer);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkBindBufferMemory2, (VkDevice)&deviceObject->id, bindInfoCount, pBindInfos);
    VT_SEND_CHECKED(REQUEST_CODE_VK_BIND_BUFFER_MEMORY2, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkBindImageMemory2, (VkDevice)&deviceObject->id, bindInfoCount, pBindInfos);
    VT_SEND_CHECKED(REQUEST_CODE_VK_BIND_IMAGE_MEMORY2, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDeviceMask, REQUEST_CODE_VK_CMD_SET_DEVICE_MASK, batch, (VkCommandBuffer)&commandBufferObject->id, deviceMask);
}

VkResult vt_call_vkGetDeviceGroupPresentCapabilitiesKHR(VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) {
    memset(pDeviceGroupPresentCapabilities->presentMask, 0, sizeof(pDeviceGroupPresentCapabilities->presentMask));
    pDeviceGroupPresentCapabilities->presentMask[0] = 0x1;
    pDeviceGroupPresentCapabilities->modes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
    
    return VK_SUCCESS;
}

VkResult vt_call_vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) {
    *pModes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
    return VK_SUCCESS;
}

VkResult vt_call_vkAcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) {
    VT_CALL_LOCK();
 
    VT_SERIALIZE_CMD(VkAcquireNextImageInfoKHR, pAcquireInfo);
    VT_SEND_CHECKED(REQUEST_CODE_VK_ACQUIRE_NEXT_IMAGE2_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    if (result >= 0 && result <= 10) {
        *pImageIndex = result;
        result = VK_SUCCESS;
    }

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDispatchBase, REQUEST_CODE_VK_CMD_DISPATCH_BASE, batch, (VkCommandBuffer)&commandBufferObject->id, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

VkResult vt_call_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t* pRectCount, VkRect2D* pRects) {
    VT_CALL_LOCK();
    if (!pRects) *pRectCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    VkObject* surfaceObject = VkObject_fromHandle(surface);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDevicePresentRectanglesKHR, (VkPhysicalDevice)&physicalDeviceObject->id, (VkSurfaceKHR)&surfaceObject->id, pRectCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_PRESENT_RECTANGLES_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    vt_unserialize_vkGetPhysicalDevicePresentRectanglesKHR(VK_NULL_HANDLE, VK_NULL_HANDLE, pRectCount, pRects, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    VT_CALL_LOCK();    
    DescriptorUpdateTemplateInfo* descriptorUpdateTemplateInfo = DescriptorUpdateTemplate_create(pCreateInfo->pDescriptorUpdateEntries, pCreateInfo->descriptorUpdateEntryCount, pCreateInfo->pipelineBindPoint);
    VkObject* descriptorUpdateTemplateObject = VkObject_create(VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE, (uint64_t)descriptorUpdateTemplateInfo);
    descriptorUpdateTemplateObject->tag = descriptorUpdateTemplateInfo;
    *pDescriptorUpdateTemplate = VkObject_toHandle(descriptorUpdateTemplateObject);

    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

void vt_call_vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* descriptorUpdateTemplateObject = VkObject_fromHandle(descriptorUpdateTemplate);
    DescriptorUpdateTemplateInfo* descriptorUpdateTemplateInfo = descriptorUpdateTemplateObject->tag;

    DescriptorUpdateTemplate_free(descriptorUpdateTemplateInfo);
    VkObject_free(descriptorUpdateTemplateObject);

    VT_CALL_UNLOCK();
}

void vt_call_vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* descriptorUpdateTemplateObject = VkObject_fromHandle(descriptorUpdateTemplate);
    DescriptorUpdateTemplateInfo* descriptorUpdateTemplateInfo = descriptorUpdateTemplateObject->tag;

    VkWriteDescriptorSet descriptorWrites[descriptorUpdateTemplateInfo->entryCount];
    VkDescriptorImageInfo imageInfos[descriptorUpdateTemplateInfo->imageInfoCount];
    VkDescriptorBufferInfo bufferInfos[descriptorUpdateTemplateInfo->bufferInfoCount];
    VkBufferView texelBufferViews[descriptorUpdateTemplateInfo->texelBufferViewCount];

    DescriptorUpdateTemplate_fillDescriptorWrites(descriptorUpdateTemplateInfo, descriptorSet, imageInfos, bufferInfos, texelBufferViews, descriptorWrites, pData);

    VT_SERIALIZE_CMD(vkUpdateDescriptorSets, (VkDevice)&deviceObject->id, descriptorUpdateTemplateInfo->entryCount, descriptorWrites, 0, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_UPDATE_DESCRIPTOR_SETS, outputBuffer, bufferSize);   
    
    VT_CALL_UNLOCK();
}

void vt_call_vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* descriptorUpdateTemplateObject = VkObject_fromHandle(descriptorUpdateTemplate);
    VkObject* layoutObject = VkObject_fromHandle(layout);
    DescriptorUpdateTemplateInfo* descriptorUpdateTemplateInfo = descriptorUpdateTemplateObject->tag;

    VkWriteDescriptorSet descriptorWrites[descriptorUpdateTemplateInfo->entryCount];
    VkDescriptorImageInfo imageInfos[descriptorUpdateTemplateInfo->imageInfoCount];
    VkDescriptorBufferInfo bufferInfos[descriptorUpdateTemplateInfo->bufferInfoCount];
    VkBufferView texelBufferViews[descriptorUpdateTemplateInfo->texelBufferViewCount];

    DescriptorUpdateTemplate_fillDescriptorWrites(descriptorUpdateTemplateInfo, VK_NULL_HANDLE, imageInfos, bufferInfos, texelBufferViews, descriptorWrites, pData);
    
    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdPushDescriptorSetKHR, REQUEST_CODE_VK_CMD_PUSH_DESCRIPTOR_SET_KHR, batch, (VkCommandBuffer)&commandBufferObject->id, descriptorUpdateTemplateInfo->pipelineBindPoint, (VkPipelineLayout)&layoutObject->id, set, descriptorUpdateTemplateInfo->entryCount, descriptorWrites);
}

void vt_call_vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetSampleLocationsEXT, REQUEST_CODE_VK_CMD_SET_SAMPLE_LOCATIONS_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, pSampleLocationsInfo);
}

void vt_call_vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) {
    VT_CALL_LOCK();
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);
    
    VT_SERIALIZE_CMD(vkGetPhysicalDeviceMultisamplePropertiesEXT, (VkPhysicalDevice)&physicalDeviceObject->id, samples, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_MULTISAMPLE_PROPERTIES_EXT);
    VT_RECV_CHECKED();

    vt_unserialize_VkMultisamplePropertiesEXT(pMultisampleProperties, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetBufferMemoryRequirements2, (VkDevice)&deviceObject->id, pInfo, pMemoryRequirements);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_BUFFER_MEMORY_REQUIREMENTS2);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkMemoryRequirements2(pMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetImageMemoryRequirements2, (VkDevice)&deviceObject->id, pInfo, pMemoryRequirements);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_IMAGE_MEMORY_REQUIREMENTS2);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkMemoryRequirements2(pMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    VT_CALL_LOCK();
    if (!pSparseMemoryRequirements) *pSparseMemoryRequirementCount = 0;
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetImageSparseMemoryRequirements2, (VkDevice)&deviceObject->id, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_IMAGE_SPARSE_MEMORY_REQUIREMENTS2);
    VT_RECV_CHECKED();

    vt_unserialize_vkGetImageSparseMemoryRequirements2(VK_NULL_HANDLE, NULL, pSparseMemoryRequirementCount, pSparseMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDeviceBufferMemoryRequirements, (VkDevice)&deviceObject->id, pInfo, pMemoryRequirements);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_BUFFER_MEMORY_REQUIREMENTS);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkMemoryRequirements2(pMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDeviceImageMemoryRequirements, (VkDevice)&deviceObject->id, pInfo, pMemoryRequirements);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_IMAGE_MEMORY_REQUIREMENTS);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkMemoryRequirements2(pMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    VT_CALL_LOCK();
    if (!pSparseMemoryRequirements) *pSparseMemoryRequirementCount = 0;
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDeviceImageSparseMemoryRequirements, (VkDevice)&deviceObject->id, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_IMAGE_SPARSE_MEMORY_REQUIREMENTS);
    VT_RECV_CHECKED();

    vt_unserialize_vkGetDeviceImageSparseMemoryRequirements(VK_NULL_HANDLE, NULL, pSparseMemoryRequirementCount, pSparseMemoryRequirements, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateSamplerYcbcrConversion, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_SAMPLER_YCBCR_CONVERSION, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t ycbcrConversionId;
    vt_unserialize_VkSamplerYcbcrConversion((VkSamplerYcbcrConversion)&ycbcrConversionId, inputBuffer, &globalMemoryPool);
    VkObject* ycbcrConversionObject = VkObject_create(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION, ycbcrConversionId);
    *pYcbcrConversion = VkObject_toHandle(ycbcrConversionObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* ycbcrConversionObject = VkObject_fromHandle(ycbcrConversion);

    VT_SERIALIZE_CMD(vkDestroySamplerYcbcrConversion, (VkDevice)&deviceObject->id, (VkSamplerYcbcrConversion)&ycbcrConversionObject->id, NULL);
    vt_send(serverRing, REQUEST_CODE_VK_DESTROY_SAMPLER_YCBCR_CONVERSION, outputBuffer, bufferSize);

    VkObject_free(ycbcrConversionObject);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDeviceQueue2, (VkDevice)&deviceObject->id, pQueueInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_QUEUE2);
    VT_RECV_CHECKED();
    
    uint64_t queueId;
    vt_unserialize_VkQueue((VkQueue)&queueId, inputBuffer, &globalMemoryPool);
    VkObject* queueObject = VkObject_create(VK_OBJECT_TYPE_QUEUE, queueId);
    *pQueue = VkObject_toHandle(queueObject);
       
    VT_CALL_UNLOCK();
}

void vt_call_vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDescriptorSetLayoutSupport, (VkDevice)&deviceObject->id, pCreateInfo, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DESCRIPTOR_SET_LAYOUT_SUPPORT);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkDescriptorSetLayoutSupport(pSupport, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice physicalDevice, uint32_t* pTimeDomainCount, VkTimeDomainKHR* pTimeDomains) {
    VT_CALL_LOCK();
    if (!pTimeDomains) *pTimeDomainCount = 0;
    VkObject* physicalDeviceObject = VkObject_fromHandle(physicalDevice);

    VT_SERIALIZE_CMD(vkGetPhysicalDeviceCalibrateableTimeDomainsKHR, (VkPhysicalDevice)&physicalDeviceObject->id, pTimeDomainCount, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_PHYSICAL_DEVICE_CALIBRATEABLE_TIME_DOMAINS_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VK_NULL_HANDLE, pTimeDomainCount, pTimeDomains, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkGetCalibratedTimestampsKHR(VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoKHR* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkGetCalibratedTimestampsKHR, (VkDevice)&deviceObject->id, timestampCount, pTimestampInfos, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_CALIBRATED_TIMESTAMPS_KHR, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    vt_unserialize_vkGetCalibratedTimestampsKHR(VK_NULL_HANDLE, &timestampCount, NULL, pTimestamps, pMaxDeviation, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);

    VT_SERIALIZE_CMD(vkCreateRenderPass2, (VkDevice)&deviceObject->id, pCreateInfo, NULL, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_CREATE_RENDER_PASS2, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);

    uint64_t renderPassId;
    vt_unserialize_VkRenderPass((VkRenderPass)&renderPassId, inputBuffer, &globalMemoryPool);
    VkObject* renderPassObject = VkObject_create(VK_OBJECT_TYPE_RENDER_PASS, renderPassId);
    *pRenderPass = VkObject_toHandle(renderPassObject);

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBeginRenderPass2, REQUEST_CODE_VK_CMD_BEGIN_RENDER_PASS2, batch, (VkCommandBuffer)&commandBufferObject->id, pRenderPassBegin, pSubpassBeginInfo);
}

void vt_call_vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdNextSubpass2, REQUEST_CODE_VK_CMD_NEXT_SUBPASS2, batch, (VkCommandBuffer)&commandBufferObject->id, pSubpassBeginInfo, pSubpassEndInfo);
}

void vt_call_vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdEndRenderPass2, REQUEST_CODE_VK_CMD_END_RENDER_PASS2, batch, (VkCommandBuffer)&commandBufferObject->id, pSubpassEndInfo);
}

VkResult vt_call_vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* semaphoreObject = VkObject_fromHandle(semaphore);

    VT_SERIALIZE_CMD(vkGetSemaphoreCounterValue, (VkDevice)&deviceObject->id, (VkSemaphore)&semaphoreObject->id, NULL);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_SEMAPHORE_COUNTER_VALUE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    *pValue = *(uint64_t*)(inputBuffer);
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    VkObject* deviceObject = VkObject_fromHandle(device);
    VT_WAIT_FOR_OBJECT(REQUEST_CODE_VK_WAIT_SEMAPHORES, vkWaitSemaphores, (VkDevice)&deviceObject->id, pWaitInfo, timeout);
}

VkResult vt_call_vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkSignalSemaphore, (VkDevice)&deviceObject->id, pSignalInfo);
    VT_SEND_CHECKED(REQUEST_CODE_VK_SIGNAL_SEMAPHORE, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VT_CALL_UNLOCK();
    return (VkResult)result;
}

void vt_call_vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* bufferObject = VkObject_fromHandle(buffer);
    VkObject* countBufferObject = VkObject_fromHandle(countBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDrawIndirectCount, REQUEST_CODE_VK_CMD_DRAW_INDIRECT_COUNT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&bufferObject->id, offset, (VkBuffer)&countBufferObject->id, countBufferOffset, maxDrawCount, stride);
}

void vt_call_vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* bufferObject = VkObject_fromHandle(buffer);
    VkObject* countBufferObject = VkObject_fromHandle(countBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDrawIndexedIndirectCount, REQUEST_CODE_VK_CMD_DRAW_INDEXED_INDIRECT_COUNT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkBuffer)&bufferObject->id, offset, (VkBuffer)&countBufferObject->id, countBufferOffset, maxDrawCount, stride);
}

void vt_call_vkCmdBindTransformFeedbackBuffersEXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBindTransformFeedbackBuffersEXT, REQUEST_CODE_VK_CMD_BIND_TRANSFORM_FEEDBACK_BUFFERS_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
}

void vt_call_vkCmdBeginTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBeginTransformFeedbackEXT, REQUEST_CODE_VK_CMD_BEGIN_TRANSFORM_FEEDBACK_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

void vt_call_vkCmdEndTransformFeedbackEXT(VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdEndTransformFeedbackEXT, REQUEST_CODE_VK_CMD_END_TRANSFORM_FEEDBACK_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

void vt_call_vkCmdBeginQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBeginQueryIndexedEXT, REQUEST_CODE_VK_CMD_BEGIN_QUERY_INDEXED_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkQueryPool)&queryPoolObject->id, query, flags, index);
}

void vt_call_vkCmdEndQueryIndexedEXT(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdEndQueryIndexedEXT, REQUEST_CODE_VK_CMD_END_QUERY_INDEXED_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, (VkQueryPool)&queryPoolObject->id, query, index);
}

void vt_call_vkCmdDrawIndirectByteCountEXT(VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* counterBufferObject = VkObject_fromHandle(counterBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdDrawIndirectByteCountEXT, REQUEST_CODE_VK_CMD_DRAW_INDIRECT_BYTE_COUNT_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, instanceCount, firstInstance, (VkBuffer)&counterBufferObject->id, counterBufferOffset, counterOffset, vertexStride);
}

uint64_t vt_call_vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetBufferOpaqueCaptureAddress, (VkDevice)&deviceObject->id, pInfo);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_BUFFER_OPAQUE_CAPTURE_ADDRESS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    uint64_t value = *(uint64_t*)(inputBuffer);
    VT_CALL_UNLOCK();
    return value;
}

VkDeviceAddress vt_call_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetBufferDeviceAddress, (VkDevice)&deviceObject->id, pInfo);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_BUFFER_DEVICE_ADDRESS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    VkDeviceAddress value = *(VkDeviceAddress*)(inputBuffer);
    VT_CALL_UNLOCK();
    return value;
}

uint64_t vt_call_vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetDeviceMemoryOpaqueCaptureAddress, (VkDevice)&deviceObject->id, pInfo);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS, VT_RETURN);
    VT_RECV_CHECKED(VT_RETURN);
    
    uint64_t value = *(uint64_t*)(inputBuffer);
    VT_CALL_UNLOCK();
    return value;
}

void vt_call_vkCmdSetLineStippleKHR(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetLineStippleKHR, REQUEST_CODE_VK_CMD_SET_LINE_STIPPLE_KHR, batch, (VkCommandBuffer)&commandBufferObject->id, lineStippleFactor, lineStipplePattern);
}

VkResult vt_call_vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
    *pToolCount = 0;
    return VK_SUCCESS;
}

void vt_call_vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetCullMode, REQUEST_CODE_VK_CMD_SET_CULL_MODE, batch, (VkCommandBuffer)&commandBufferObject->id, cullMode);
}

void vt_call_vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetFrontFace, REQUEST_CODE_VK_CMD_SET_FRONT_FACE, batch, (VkCommandBuffer)&commandBufferObject->id, frontFace);
}

void vt_call_vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetPrimitiveTopology, REQUEST_CODE_VK_CMD_SET_PRIMITIVE_TOPOLOGY, batch, (VkCommandBuffer)&commandBufferObject->id, primitiveTopology);
}

void vt_call_vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetViewportWithCount, REQUEST_CODE_VK_CMD_SET_VIEWPORT_WITH_COUNT, batch, (VkCommandBuffer)&commandBufferObject->id, viewportCount, pViewports);
}

void vt_call_vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetScissorWithCount, REQUEST_CODE_VK_CMD_SET_SCISSOR_WITH_COUNT, batch, (VkCommandBuffer)&commandBufferObject->id, scissorCount, pScissors);
}

void vt_call_vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBindVertexBuffers2, REQUEST_CODE_VK_CMD_BIND_VERTEX_BUFFERS2, batch, (VkCommandBuffer)&commandBufferObject->id, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

void vt_call_vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthTestEnable, REQUEST_CODE_VK_CMD_SET_DEPTH_TEST_ENABLE, batch, (VkCommandBuffer)&commandBufferObject->id, depthTestEnable);
}

void vt_call_vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthWriteEnable, REQUEST_CODE_VK_CMD_SET_DEPTH_WRITE_ENABLE, batch, (VkCommandBuffer)&commandBufferObject->id, depthWriteEnable);
}

void vt_call_vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthCompareOp, REQUEST_CODE_VK_CMD_SET_DEPTH_COMPARE_OP, batch, (VkCommandBuffer)&commandBufferObject->id, depthCompareOp);
}

void vt_call_vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthBoundsTestEnable, REQUEST_CODE_VK_CMD_SET_DEPTH_BOUNDS_TEST_ENABLE, batch, (VkCommandBuffer)&commandBufferObject->id, depthBoundsTestEnable);
}

void vt_call_vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetStencilTestEnable, REQUEST_CODE_VK_CMD_SET_STENCIL_TEST_ENABLE, batch, (VkCommandBuffer)&commandBufferObject->id, stencilTestEnable);
}

void vt_call_vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetStencilOp, REQUEST_CODE_VK_CMD_SET_STENCIL_OP, batch, (VkCommandBuffer)&commandBufferObject->id, faceMask, failOp, passOp, depthFailOp, compareOp);
}

void vt_call_vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetRasterizerDiscardEnable, REQUEST_CODE_VK_CMD_SET_RASTERIZER_DISCARD_ENABLE, batch, (VkCommandBuffer)&commandBufferObject->id, rasterizerDiscardEnable);
}

void vt_call_vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthBiasEnable, REQUEST_CODE_VK_CMD_SET_DEPTH_BIAS_ENABLE, batch, (VkCommandBuffer)&commandBufferObject->id, depthBiasEnable);
}

void vt_call_vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetPrimitiveRestartEnable, REQUEST_CODE_VK_CMD_SET_PRIMITIVE_RESTART_ENABLE, batch, (VkCommandBuffer)&commandBufferObject->id, primitiveRestartEnable);
}

void vt_call_vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetTessellationDomainOriginEXT, REQUEST_CODE_VK_CMD_SET_TESSELLATION_DOMAIN_ORIGIN_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, domainOrigin);
}

void vt_call_vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthClampEnableEXT, REQUEST_CODE_VK_CMD_SET_DEPTH_CLAMP_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, depthClampEnable);
}

void vt_call_vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetPolygonModeEXT, REQUEST_CODE_VK_CMD_SET_POLYGON_MODE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, polygonMode);
}

void vt_call_vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetRasterizationSamplesEXT, REQUEST_CODE_VK_CMD_SET_RASTERIZATION_SAMPLES_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, rasterizationSamples);
}

void vt_call_vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetSampleMaskEXT, REQUEST_CODE_VK_CMD_SET_SAMPLE_MASK_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, samples, pSampleMask);
}

void vt_call_vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetAlphaToCoverageEnableEXT, REQUEST_CODE_VK_CMD_SET_ALPHA_TO_COVERAGE_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, alphaToCoverageEnable);
}

void vt_call_vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetAlphaToOneEnableEXT, REQUEST_CODE_VK_CMD_SET_ALPHA_TO_ONE_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, alphaToOneEnable);
}

void vt_call_vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetLogicOpEnableEXT, REQUEST_CODE_VK_CMD_SET_LOGIC_OP_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, logicOpEnable);
}

void vt_call_vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetColorBlendEnableEXT, REQUEST_CODE_VK_CMD_SET_COLOR_BLEND_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, firstAttachment, attachmentCount, pColorBlendEnables);
}

void vt_call_vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetColorBlendEquationEXT, REQUEST_CODE_VK_CMD_SET_COLOR_BLEND_EQUATION_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, firstAttachment, attachmentCount, pColorBlendEquations);
}

void vt_call_vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetColorWriteMaskEXT, REQUEST_CODE_VK_CMD_SET_COLOR_WRITE_MASK_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, firstAttachment, attachmentCount, pColorWriteMasks);
}

void vt_call_vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetRasterizationStreamEXT, REQUEST_CODE_VK_CMD_SET_RASTERIZATION_STREAM_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, rasterizationStream);
}

void vt_call_vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetConservativeRasterizationModeEXT, REQUEST_CODE_VK_CMD_SET_CONSERVATIVE_RASTERIZATION_MODE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, conservativeRasterizationMode);
}

void vt_call_vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetExtraPrimitiveOverestimationSizeEXT, REQUEST_CODE_VK_CMD_SET_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, extraPrimitiveOverestimationSize);
}

void vt_call_vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthClipEnableEXT, REQUEST_CODE_VK_CMD_SET_DEPTH_CLIP_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, depthClipEnable);
}

void vt_call_vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetSampleLocationsEnableEXT, REQUEST_CODE_VK_CMD_SET_SAMPLE_LOCATIONS_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, sampleLocationsEnable);
}

void vt_call_vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetColorBlendAdvancedEXT, REQUEST_CODE_VK_CMD_SET_COLOR_BLEND_ADVANCED_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, firstAttachment, attachmentCount, pColorBlendAdvanced);
}

void vt_call_vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetProvokingVertexModeEXT, REQUEST_CODE_VK_CMD_SET_PROVOKING_VERTEX_MODE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, provokingVertexMode);
}

void vt_call_vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetLineRasterizationModeEXT, REQUEST_CODE_VK_CMD_SET_LINE_RASTERIZATION_MODE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, lineRasterizationMode);
}

void vt_call_vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetLineStippleEnableEXT, REQUEST_CODE_VK_CMD_SET_LINE_STIPPLE_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, stippledLineEnable);
}

void vt_call_vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetDepthClipNegativeOneToOneEXT, REQUEST_CODE_VK_CMD_SET_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, negativeOneToOne);
}

VkResult vt_call_vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) {
    VT_CALL_LOCK();
    ArrayList* dataList = calloc(1, sizeof(ArrayList)); 
    VkObject* privateDataSlotObject = VkObject_create(VK_OBJECT_TYPE_PRIVATE_DATA_SLOT, (uint64_t)dataList);
    privateDataSlotObject->tag = dataList;
    *pPrivateDataSlot = VkObject_toHandle(privateDataSlotObject);

    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

void vt_call_vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) {
    VT_CALL_LOCK();
    VkObject* privateDataSlotObject = VkObject_fromHandle(privateDataSlot);
    ArrayList* dataList = privateDataSlotObject->tag;
    
    ArrayList_free(dataList, true);
    VkObject_free(privateDataSlotObject);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
    VT_CALL_LOCK();
    VkObject* privateDataSlotObject = VkObject_fromHandle(privateDataSlot);
    ArrayList* dataList = privateDataSlotObject->tag;
    
    uint64_t* slot = NULL;
    for (int i = 0; i < dataList->size; i++) {
        if (((uint64_t*)dataList->elements[i])[0] == objectHandle) {
            slot = dataList->elements[i];
            break;
        }
    }
    
    if (!slot) {
        slot = malloc(2 * sizeof(uint64_t));
        ArrayList_add(dataList, slot);
    }
    
    slot[0] = objectHandle;
    slot[1] = data;
    
    VT_CALL_UNLOCK();
    return VK_SUCCESS;
}

void vt_call_vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
    VT_CALL_LOCK();
    VkObject* privateDataSlotObject = VkObject_fromHandle(privateDataSlot);
    ArrayList* dataList = privateDataSlotObject->tag;
    
    *pData = 0;
    for (int i = 0; i < dataList->size; i++) {
        if (((uint64_t*)dataList->elements[i])[0] == objectHandle) {
            *pData = ((uint64_t*)dataList->elements[i])[1];
            break;
        }
    }
    
    VT_CALL_UNLOCK();
}

void vt_call_vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyBuffer2, REQUEST_CODE_VK_CMD_COPY_BUFFER2, batch, (VkCommandBuffer)&commandBufferObject->id, pCopyBufferInfo);
}

void vt_call_vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyImage2, REQUEST_CODE_VK_CMD_COPY_IMAGE2, batch, (VkCommandBuffer)&commandBufferObject->id, pCopyImageInfo);
}

void vt_call_vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBlitImage2, REQUEST_CODE_VK_CMD_BLIT_IMAGE2, batch, (VkCommandBuffer)&commandBufferObject->id, pBlitImageInfo);
}

void vt_call_vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyBufferToImage2, REQUEST_CODE_VK_CMD_COPY_BUFFER_TO_IMAGE2, batch, (VkCommandBuffer)&commandBufferObject->id, pCopyBufferToImageInfo);
}

void vt_call_vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdCopyImageToBuffer2, REQUEST_CODE_VK_CMD_COPY_IMAGE_TO_BUFFER2, batch, (VkCommandBuffer)&commandBufferObject->id, pCopyImageToBufferInfo);
}

void vt_call_vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdResolveImage2, REQUEST_CODE_VK_CMD_RESOLVE_IMAGE2, batch, (VkCommandBuffer)&commandBufferObject->id, pResolveImageInfo);
}

void vt_call_vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetColorWriteEnableEXT, REQUEST_CODE_VK_CMD_SET_COLOR_WRITE_ENABLE_EXT, batch, (VkCommandBuffer)&commandBufferObject->id, attachmentCount, pColorWriteEnables);
}

void vt_call_vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* eventObject = VkObject_fromHandle(event);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdSetEvent2, REQUEST_CODE_VK_CMD_SET_EVENT2, batch, (VkCommandBuffer)&commandBufferObject->id, (VkEvent)&eventObject->id, pDependencyInfo);
}

void vt_call_vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* eventObject = VkObject_fromHandle(event);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdResetEvent2, REQUEST_CODE_VK_CMD_RESET_EVENT2, batch, (VkCommandBuffer)&commandBufferObject->id, (VkEvent)&eventObject->id, stageMask);
}

void vt_call_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdWaitEvents2, REQUEST_CODE_VK_CMD_WAIT_EVENTS2, batch, (VkCommandBuffer)&commandBufferObject->id, eventCount, pEvents, pDependencyInfos);
}

void vt_call_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdPipelineBarrier2, REQUEST_CODE_VK_CMD_PIPELINE_BARRIER2, batch, (VkCommandBuffer)&commandBufferObject->id, pDependencyInfo);
}

VkResult vt_call_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    VT_CALL_LOCK();
    VkObject* queueObject = VkObject_fromHandle(queue);
    VkObject* fenceObject = VkObject_fromHandle(fence);
    
    bool shouldWait = RingBuffer_hasStatus(clientRing, RING_STATUS_WAIT);
    
    VT_SERIALIZE_CMD(vkQueueSubmit2, (VkQueue)&queueObject->id, submitCount, pSubmits, (VkFence)&fenceObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_QUEUE_SUBMIT2, VT_RETURN);
    
    if (shouldWait) {
        VT_RECV_CHECKED(VT_RETURN);
        VT_CALL_UNLOCK();
        return (VkResult)result;
    }
    else {
        VT_CALL_UNLOCK();
        return VK_SUCCESS;
    }
}

void vt_call_vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);
    VkObject* queryPoolObject = VkObject_fromHandle(queryPool);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdWriteTimestamp2, REQUEST_CODE_VK_CMD_WRITE_TIMESTAMP2, batch, (VkCommandBuffer)&commandBufferObject->id, stage, (VkQueryPool)&queryPoolObject->id, query);
}

void vt_call_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(vkCmdBeginRendering, REQUEST_CODE_VK_CMD_BEGIN_RENDERING, batch, (VkCommandBuffer)&commandBufferObject->id, pRenderingInfo);
}

void vt_call_vkCmdEndRendering(VkCommandBuffer commandBuffer) {
    VkObject* commandBufferObject = VkObject_fromHandle(commandBuffer);

    CommandBatch* batch = commandBufferObject->tag;
    VT_CMD_ENQUEUE(VkCommandBuffer, REQUEST_CODE_VK_CMD_END_RENDERING, batch, (VkCommandBuffer)&commandBufferObject->id);
}

void vt_call_vkGetShaderModuleIdentifierEXT(VkDevice device, VkShaderModule shaderModule, VkShaderModuleIdentifierEXT* pIdentifier) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    VkObject* shaderModuleObject = VkObject_fromHandle(shaderModule);
    
    VT_SERIALIZE_CMD(vkGetShaderModuleIdentifierEXT, (VkDevice)&deviceObject->id, (VkShaderModule)&shaderModuleObject->id, pIdentifier);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_SHADER_MODULE_IDENTIFIER_EXT);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkShaderModuleIdentifierEXT(pIdentifier, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

void vt_call_vkGetShaderModuleCreateInfoIdentifierEXT(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, VkShaderModuleIdentifierEXT* pIdentifier) {
    VT_CALL_LOCK();
    VkObject* deviceObject = VkObject_fromHandle(device);
    
    VT_SERIALIZE_CMD(vkGetShaderModuleCreateInfoIdentifierEXT, (VkDevice)&deviceObject->id, pCreateInfo, pIdentifier);
    VT_SEND_CHECKED(REQUEST_CODE_VK_GET_SHADER_MODULE_CREATE_INFO_IDENTIFIER_EXT);
    VT_RECV_CHECKED();
    
    vt_unserialize_VkShaderModuleIdentifierEXT(pIdentifier, inputBuffer, &globalMemoryPool);
    VT_CALL_UNLOCK();
}

VkResult vt_call_vkMapMemory2KHR(VkDevice device, const VkMemoryMapInfoKHR* pMemoryMapInfo, void** ppData) {
    VT_CALL_LOCK();
    VkObject* memoryObject = VkObject_fromHandle(pMemoryMapInfo->memory);

    MappedMemory* mappedMemory = memoryObject->tag;
    if (mappedMemory->data) {
        VT_CALL_UNLOCK();
        return VK_SUCCESS;
    }

    VT_SERIALIZE_CMD(VkDeviceMemory, (VkDeviceMemory)&memoryObject->id);
    VT_SEND_CHECKED(REQUEST_CODE_VK_MAP_MEMORY, VT_RETURN);

    int fd, result, numFds;
    recv_fds(serverFd, &fd, &numFds, &result, sizeof(VkResult));
    if (numFds == 1) {
        mappedMemory->size = pMemoryMapInfo->size;
        if (mappedMemory->size == VK_WHOLE_SIZE) mappedMemory->size = mappedMemory->allocationSize;

        VkMemoryMapPlacedInfoEXT* placedInfo = findNextVkStructure(pMemoryMapInfo->pNext, VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT);
        void* placedAddr = placedInfo ? placedInfo->pPlacedAddress : NULL;

        void* data = mmap(placedAddr, mappedMemory->size, PROT_WRITE | PROT_READ, MAP_SHARED | (placedAddr ? MAP_FIXED : 0), fd, pMemoryMapInfo->offset);
        if (data != MAP_FAILED) {
            CLOSEFD(fd);
            mappedMemory->data = data;

            if (!placedAddr) *ppData = data;
        }
        else result = VK_ERROR_MEMORY_MAP_FAILED;
    }
    else result = VK_ERROR_MEMORY_MAP_FAILED;

    VT_CALL_UNLOCK();
    return (VkResult)result;
}

VkResult vt_call_vkUnmapMemory2KHR(VkDevice device, const VkMemoryUnmapInfoKHR* pMemoryUnmapInfo) {
    vt_call_vkUnmapMemory(device, pMemoryUnmapInfo->memory);
    return VK_SUCCESS;
}

static const struct VulkanFunc vkDispatchTable[] = {
    {"vkCreateInstance", vt_call_vkCreateInstance},
    {"vkDestroyInstance", vt_call_vkDestroyInstance},
    {"vkEnumeratePhysicalDevices", vt_call_vkEnumeratePhysicalDevices},
    {"vkGetDeviceProcAddr", vt_call_vkGetDeviceProcAddr},
    {"vkGetInstanceProcAddr", vt_call_vkGetInstanceProcAddr},
    {"vkGetPhysicalDeviceProperties", vt_call_vkGetPhysicalDeviceProperties},
    {"vkGetPhysicalDeviceQueueFamilyProperties", vt_call_vkGetPhysicalDeviceQueueFamilyProperties},
    {"vkGetPhysicalDeviceMemoryProperties", vt_call_vkGetPhysicalDeviceMemoryProperties},
    {"vkGetPhysicalDeviceFeatures", vt_call_vkGetPhysicalDeviceFeatures},
    {"vkGetPhysicalDeviceFormatProperties", vt_call_vkGetPhysicalDeviceFormatProperties},
    {"vkGetPhysicalDeviceImageFormatProperties", vt_call_vkGetPhysicalDeviceImageFormatProperties},
    {"vkCreateDevice", vt_call_vkCreateDevice},
    {"vkDestroyDevice", vt_call_vkDestroyDevice},
    {"vkEnumerateInstanceVersion", vt_call_vkEnumerateInstanceVersion},
    {"vkEnumerateInstanceLayerProperties", vt_call_vkEnumerateInstanceLayerProperties},
    {"vkEnumerateInstanceExtensionProperties", vt_call_vkEnumerateInstanceExtensionProperties},
    {"vkEnumerateDeviceLayerProperties", vt_call_vkEnumerateDeviceLayerProperties},
    {"vkEnumerateDeviceExtensionProperties", vt_call_vkEnumerateDeviceExtensionProperties},
    {"vkGetDeviceQueue", vt_call_vkGetDeviceQueue},
    {"vkQueueSubmit", vt_call_vkQueueSubmit},
    {"vkQueueWaitIdle", vt_call_vkQueueWaitIdle},
    {"vkDeviceWaitIdle", vt_call_vkDeviceWaitIdle},
    {"vkAllocateMemory", vt_call_vkAllocateMemory},
    {"vkFreeMemory", vt_call_vkFreeMemory},
    {"vkMapMemory", vt_call_vkMapMemory},
    {"vkUnmapMemory", vt_call_vkUnmapMemory},
    {"vkFlushMappedMemoryRanges", vt_call_vkFlushMappedMemoryRanges},
    {"vkInvalidateMappedMemoryRanges", vt_call_vkInvalidateMappedMemoryRanges},
    {"vkGetDeviceMemoryCommitment", vt_call_vkGetDeviceMemoryCommitment},
    {"vkGetBufferMemoryRequirements", vt_call_vkGetBufferMemoryRequirements},
    {"vkBindBufferMemory", vt_call_vkBindBufferMemory},
    {"vkGetImageMemoryRequirements", vt_call_vkGetImageMemoryRequirements},
    {"vkBindImageMemory", vt_call_vkBindImageMemory},
    {"vkGetImageSparseMemoryRequirements", vt_call_vkGetImageSparseMemoryRequirements},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", vt_call_vkGetPhysicalDeviceSparseImageFormatProperties},
    {"vkQueueBindSparse", vt_call_vkQueueBindSparse},
    {"vkCreateFence", vt_call_vkCreateFence},
    {"vkDestroyFence", vt_call_vkDestroyFence},
    {"vkResetFences", vt_call_vkResetFences},
    {"vkGetFenceStatus", vt_call_vkGetFenceStatus},
    {"vkWaitForFences", vt_call_vkWaitForFences},
    {"vkCreateSemaphore", vt_call_vkCreateSemaphore},
    {"vkDestroySemaphore", vt_call_vkDestroySemaphore},
    {"vkCreateEvent", vt_call_vkCreateEvent},
    {"vkDestroyEvent", vt_call_vkDestroyEvent},
    {"vkGetEventStatus", vt_call_vkGetEventStatus},
    {"vkSetEvent", vt_call_vkSetEvent},
    {"vkResetEvent", vt_call_vkResetEvent},
    {"vkCreateQueryPool", vt_call_vkCreateQueryPool},
    {"vkDestroyQueryPool", vt_call_vkDestroyQueryPool},
    {"vkGetQueryPoolResults", vt_call_vkGetQueryPoolResults},
    {"vkResetQueryPool", vt_call_vkResetQueryPool},
    {"vkResetQueryPoolEXT", vt_call_vkResetQueryPool},
    {"vkCreateBuffer", vt_call_vkCreateBuffer},
    {"vkDestroyBuffer", vt_call_vkDestroyBuffer},
    {"vkCreateBufferView", vt_call_vkCreateBufferView},
    {"vkDestroyBufferView", vt_call_vkDestroyBufferView},
    {"vkCreateImage", vt_call_vkCreateImage},
    {"vkDestroyImage", vt_call_vkDestroyImage},
    {"vkGetImageSubresourceLayout", vt_call_vkGetImageSubresourceLayout},
    {"vkCreateImageView", vt_call_vkCreateImageView},
    {"vkDestroyImageView", vt_call_vkDestroyImageView},
    {"vkCreateShaderModule", vt_call_vkCreateShaderModule},
    {"vkDestroyShaderModule", vt_call_vkDestroyShaderModule},
    {"vkCreatePipelineCache", vt_call_vkCreatePipelineCache},
    {"vkDestroyPipelineCache", vt_call_vkDestroyPipelineCache},
    {"vkGetPipelineCacheData", vt_call_vkGetPipelineCacheData},
    {"vkMergePipelineCaches", vt_call_vkMergePipelineCaches},
    {"vkCreateGraphicsPipelines", vt_call_vkCreateGraphicsPipelines},
    {"vkCreateComputePipelines", vt_call_vkCreateComputePipelines},
    {"vkDestroyPipeline", vt_call_vkDestroyPipeline},
    {"vkCreatePipelineLayout", vt_call_vkCreatePipelineLayout},
    {"vkDestroyPipelineLayout", vt_call_vkDestroyPipelineLayout},
    {"vkCreateSampler", vt_call_vkCreateSampler},
    {"vkDestroySampler", vt_call_vkDestroySampler},
    {"vkCreateDescriptorSetLayout", vt_call_vkCreateDescriptorSetLayout},
    {"vkDestroyDescriptorSetLayout", vt_call_vkDestroyDescriptorSetLayout},
    {"vkCreateDescriptorPool", vt_call_vkCreateDescriptorPool},
    {"vkDestroyDescriptorPool", vt_call_vkDestroyDescriptorPool},
    {"vkResetDescriptorPool", vt_call_vkResetDescriptorPool},
    {"vkAllocateDescriptorSets", vt_call_vkAllocateDescriptorSets},
    {"vkFreeDescriptorSets", vt_call_vkFreeDescriptorSets},
    {"vkUpdateDescriptorSets", vt_call_vkUpdateDescriptorSets},
    {"vkCreateFramebuffer", vt_call_vkCreateFramebuffer},
    {"vkDestroyFramebuffer", vt_call_vkDestroyFramebuffer},
    {"vkCreateRenderPass", vt_call_vkCreateRenderPass},
    {"vkDestroyRenderPass", vt_call_vkDestroyRenderPass},
    {"vkGetRenderAreaGranularity", vt_call_vkGetRenderAreaGranularity},
    {"vkCreateCommandPool", vt_call_vkCreateCommandPool},
    {"vkDestroyCommandPool", vt_call_vkDestroyCommandPool},
    {"vkResetCommandPool", vt_call_vkResetCommandPool},
    {"vkAllocateCommandBuffers", vt_call_vkAllocateCommandBuffers},
    {"vkFreeCommandBuffers", vt_call_vkFreeCommandBuffers},
    {"vkBeginCommandBuffer", vt_call_vkBeginCommandBuffer},
    {"vkEndCommandBuffer", vt_call_vkEndCommandBuffer},
    {"vkResetCommandBuffer", vt_call_vkResetCommandBuffer},
    {"vkCmdBindPipeline", vt_call_vkCmdBindPipeline},
    {"vkCmdSetViewport", vt_call_vkCmdSetViewport},
    {"vkCmdSetScissor", vt_call_vkCmdSetScissor},
    {"vkCmdSetLineWidth", vt_call_vkCmdSetLineWidth},
    {"vkCmdSetDepthBias", vt_call_vkCmdSetDepthBias},
    {"vkCmdSetBlendConstants", vt_call_vkCmdSetBlendConstants},
    {"vkCmdSetDepthBounds", vt_call_vkCmdSetDepthBounds},
    {"vkCmdSetStencilCompareMask", vt_call_vkCmdSetStencilCompareMask},
    {"vkCmdSetStencilWriteMask", vt_call_vkCmdSetStencilWriteMask},
    {"vkCmdSetStencilReference", vt_call_vkCmdSetStencilReference},
    {"vkCmdBindDescriptorSets", vt_call_vkCmdBindDescriptorSets},
    {"vkCmdBindIndexBuffer", vt_call_vkCmdBindIndexBuffer},
    {"vkCmdBindVertexBuffers", vt_call_vkCmdBindVertexBuffers},
    {"vkCmdDraw", vt_call_vkCmdDraw},
    {"vkCmdDrawIndexed", vt_call_vkCmdDrawIndexed},
    {"vkCmdDrawIndirect", vt_call_vkCmdDrawIndirect},
    {"vkCmdDrawIndexedIndirect", vt_call_vkCmdDrawIndexedIndirect},
    {"vkCmdDispatch", vt_call_vkCmdDispatch},
    {"vkCmdDispatchIndirect", vt_call_vkCmdDispatchIndirect},
    {"vkCmdCopyBuffer", vt_call_vkCmdCopyBuffer},
    {"vkCmdCopyImage", vt_call_vkCmdCopyImage},
    {"vkCmdBlitImage", vt_call_vkCmdBlitImage},
    {"vkCmdCopyBufferToImage", vt_call_vkCmdCopyBufferToImage},
    {"vkCmdCopyImageToBuffer", vt_call_vkCmdCopyImageToBuffer},
    {"vkCmdUpdateBuffer", vt_call_vkCmdUpdateBuffer},
    {"vkCmdFillBuffer", vt_call_vkCmdFillBuffer},
    {"vkCmdClearColorImage", vt_call_vkCmdClearColorImage},
    {"vkCmdClearDepthStencilImage", vt_call_vkCmdClearDepthStencilImage},
    {"vkCmdClearAttachments", vt_call_vkCmdClearAttachments},
    {"vkCmdResolveImage", vt_call_vkCmdResolveImage},
    {"vkCmdSetEvent", vt_call_vkCmdSetEvent},
    {"vkCmdResetEvent", vt_call_vkCmdResetEvent},
    {"vkCmdWaitEvents", vt_call_vkCmdWaitEvents},
    {"vkCmdPipelineBarrier", vt_call_vkCmdPipelineBarrier},
    {"vkCmdBeginQuery", vt_call_vkCmdBeginQuery},
    {"vkCmdEndQuery", vt_call_vkCmdEndQuery},
    {"vkCmdBeginConditionalRenderingEXT", vt_call_vkCmdBeginConditionalRenderingEXT},
    {"vkCmdEndConditionalRenderingEXT", vt_call_vkCmdEndConditionalRenderingEXT},
    {"vkCmdResetQueryPool", vt_call_vkCmdResetQueryPool},
    {"vkCmdWriteTimestamp", vt_call_vkCmdWriteTimestamp},
    {"vkCmdCopyQueryPoolResults", vt_call_vkCmdCopyQueryPoolResults},
    {"vkCmdPushConstants", vt_call_vkCmdPushConstants},
    {"vkCmdBeginRenderPass", vt_call_vkCmdBeginRenderPass},
    {"vkCmdNextSubpass", vt_call_vkCmdNextSubpass},
    {"vkCmdEndRenderPass", vt_call_vkCmdEndRenderPass},
    {"vkCmdExecuteCommands", vt_call_vkCmdExecuteCommands},
    {"vkDestroySurfaceKHR", vt_call_vkDestroySurfaceKHR},
    {"vkGetPhysicalDeviceSurfaceSupportKHR", vt_call_vkGetPhysicalDeviceSurfaceSupportKHR},
    {"vkGetPhysicalDeviceSurfaceCapabilitiesKHR", vt_call_vkGetPhysicalDeviceSurfaceCapabilitiesKHR},
    {"vkGetPhysicalDeviceSurfaceFormatsKHR", vt_call_vkGetPhysicalDeviceSurfaceFormatsKHR},
    {"vkGetPhysicalDeviceSurfacePresentModesKHR", vt_call_vkGetPhysicalDeviceSurfacePresentModesKHR},
    {"vkCreateSwapchainKHR", vt_call_vkCreateSwapchainKHR},
    {"vkDestroySwapchainKHR", vt_call_vkDestroySwapchainKHR},
    {"vkGetSwapchainImagesKHR", vt_call_vkGetSwapchainImagesKHR},
    {"vkAcquireNextImageKHR", vt_call_vkAcquireNextImageKHR},
    {"vkQueuePresentKHR", vt_call_vkQueuePresentKHR},
    {"vkCreateXlibSurfaceKHR", vt_call_vkCreateXlibSurfaceKHR},
    {"vkGetPhysicalDeviceXlibPresentationSupportKHR", vt_call_vkGetPhysicalDeviceXlibPresentationSupportKHR},
    {"vkGetPhysicalDeviceFeatures2", vt_call_vkGetPhysicalDeviceFeatures2},
    {"vkGetPhysicalDeviceFeatures2KHR", vt_call_vkGetPhysicalDeviceFeatures2},
    {"vkGetPhysicalDeviceProperties2", vt_call_vkGetPhysicalDeviceProperties2},
    {"vkGetPhysicalDeviceProperties2KHR", vt_call_vkGetPhysicalDeviceProperties2},
    {"vkGetPhysicalDeviceFormatProperties2", vt_call_vkGetPhysicalDeviceFormatProperties2},
    {"vkGetPhysicalDeviceFormatProperties2KHR", vt_call_vkGetPhysicalDeviceFormatProperties2},
    {"vkGetPhysicalDeviceImageFormatProperties2", vt_call_vkGetPhysicalDeviceImageFormatProperties2},
    {"vkGetPhysicalDeviceImageFormatProperties2KHR", vt_call_vkGetPhysicalDeviceImageFormatProperties2},
    {"vkGetPhysicalDeviceQueueFamilyProperties2", vt_call_vkGetPhysicalDeviceQueueFamilyProperties2},
    {"vkGetPhysicalDeviceQueueFamilyProperties2KHR", vt_call_vkGetPhysicalDeviceQueueFamilyProperties2},
    {"vkGetPhysicalDeviceMemoryProperties2", vt_call_vkGetPhysicalDeviceMemoryProperties2},
    {"vkGetPhysicalDeviceMemoryProperties2KHR", vt_call_vkGetPhysicalDeviceMemoryProperties2},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2", vt_call_vkGetPhysicalDeviceSparseImageFormatProperties2},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2KHR", vt_call_vkGetPhysicalDeviceSparseImageFormatProperties2},
    {"vkCmdPushDescriptorSetKHR", vt_call_vkCmdPushDescriptorSetKHR},
    {"vkTrimCommandPool", vt_call_vkTrimCommandPool},
    {"vkTrimCommandPoolKHR", vt_call_vkTrimCommandPool},
    {"vkGetPhysicalDeviceExternalBufferProperties", vt_call_vkGetPhysicalDeviceExternalBufferProperties},
    {"vkGetMemoryFdKHR", vt_call_vkGetMemoryFdKHR},
    {"vkGetMemoryFdPropertiesKHR", vt_call_vkGetMemoryFdPropertiesKHR},
    {"vkGetPhysicalDeviceExternalSemaphoreProperties", vt_call_vkGetPhysicalDeviceExternalSemaphoreProperties},
    {"vkGetSemaphoreFdKHR", vt_call_vkGetSemaphoreFdKHR},
    {"vkImportSemaphoreFdKHR", vt_call_vkImportSemaphoreFdKHR},
    {"vkGetPhysicalDeviceExternalFenceProperties", vt_call_vkGetPhysicalDeviceExternalFenceProperties},
    {"vkGetFenceFdKHR", vt_call_vkGetFenceFdKHR},
    {"vkImportFenceFdKHR", vt_call_vkImportFenceFdKHR},
    {"vkEnumeratePhysicalDeviceGroups", vt_call_vkEnumeratePhysicalDeviceGroups},
    {"vkGetDeviceGroupPeerMemoryFeatures", vt_call_vkGetDeviceGroupPeerMemoryFeatures},
    {"vkBindBufferMemory2", vt_call_vkBindBufferMemory2},
    {"vkBindImageMemory2", vt_call_vkBindImageMemory2},
    {"vkCmdSetDeviceMask", vt_call_vkCmdSetDeviceMask},
    {"vkGetDeviceGroupPresentCapabilitiesKHR", vt_call_vkGetDeviceGroupPresentCapabilitiesKHR},
    {"vkGetDeviceGroupSurfacePresentModesKHR", vt_call_vkGetDeviceGroupSurfacePresentModesKHR},
    {"vkAcquireNextImage2KHR", vt_call_vkAcquireNextImage2KHR},
    {"vkCmdDispatchBase", vt_call_vkCmdDispatchBase},
    {"vkGetPhysicalDevicePresentRectanglesKHR", vt_call_vkGetPhysicalDevicePresentRectanglesKHR},
    {"vkCreateDescriptorUpdateTemplate", vt_call_vkCreateDescriptorUpdateTemplate},
    {"vkCreateDescriptorUpdateTemplateKHR", vt_call_vkCreateDescriptorUpdateTemplate},
    {"vkDestroyDescriptorUpdateTemplate", vt_call_vkDestroyDescriptorUpdateTemplate},
    {"vkDestroyDescriptorUpdateTemplateKHR", vt_call_vkDestroyDescriptorUpdateTemplate},
    {"vkUpdateDescriptorSetWithTemplate", vt_call_vkUpdateDescriptorSetWithTemplate},
    {"vkUpdateDescriptorSetWithTemplateKHR", vt_call_vkUpdateDescriptorSetWithTemplate},
    {"vkCmdPushDescriptorSetWithTemplateKHR", vt_call_vkCmdPushDescriptorSetWithTemplateKHR},
    {"vkCmdSetSampleLocationsEXT", vt_call_vkCmdSetSampleLocationsEXT},
    {"vkGetPhysicalDeviceMultisamplePropertiesEXT", vt_call_vkGetPhysicalDeviceMultisamplePropertiesEXT},
    {"vkGetBufferMemoryRequirements2", vt_call_vkGetBufferMemoryRequirements2},
    {"vkGetBufferMemoryRequirements2KHR", vt_call_vkGetBufferMemoryRequirements2},
    {"vkGetImageMemoryRequirements2", vt_call_vkGetImageMemoryRequirements2},
    {"vkGetImageMemoryRequirements2KHR", vt_call_vkGetImageMemoryRequirements2},
    {"vkGetImageSparseMemoryRequirements2", vt_call_vkGetImageSparseMemoryRequirements2},
    {"vkGetImageSparseMemoryRequirements2KHR", vt_call_vkGetImageSparseMemoryRequirements2},
    {"vkGetDeviceBufferMemoryRequirements", vt_call_vkGetDeviceBufferMemoryRequirements},
    {"vkGetDeviceImageMemoryRequirements", vt_call_vkGetDeviceImageMemoryRequirements},
    {"vkGetDeviceImageSparseMemoryRequirements", vt_call_vkGetDeviceImageSparseMemoryRequirements},
    {"vkCreateSamplerYcbcrConversion", vt_call_vkCreateSamplerYcbcrConversion},
    {"vkCreateSamplerYcbcrConversionKHR", vt_call_vkCreateSamplerYcbcrConversion},
    {"vkDestroySamplerYcbcrConversion", vt_call_vkDestroySamplerYcbcrConversion},
    {"vkDestroySamplerYcbcrConversionKHR", vt_call_vkDestroySamplerYcbcrConversion},
    {"vkGetDeviceQueue2", vt_call_vkGetDeviceQueue2},
    {"vkGetDescriptorSetLayoutSupport", vt_call_vkGetDescriptorSetLayoutSupport},
    {"vkGetDescriptorSetLayoutSupportKHR", vt_call_vkGetDescriptorSetLayoutSupport},
    {"vkGetPhysicalDeviceCalibrateableTimeDomainsKHR", vt_call_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR},
    {"vkGetPhysicalDeviceCalibrateableTimeDomainsEXT", vt_call_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR},
    {"vkGetCalibratedTimestampsKHR", vt_call_vkGetCalibratedTimestampsKHR},
    {"vkGetCalibratedTimestampsEXT", vt_call_vkGetCalibratedTimestampsKHR},
    {"vkCreateRenderPass2", vt_call_vkCreateRenderPass2},
    {"vkCreateRenderPass2KHR", vt_call_vkCreateRenderPass2},
    {"vkCmdBeginRenderPass2", vt_call_vkCmdBeginRenderPass2},
    {"vkCmdBeginRenderPass2KHR", vt_call_vkCmdBeginRenderPass2},
    {"vkCmdNextSubpass2", vt_call_vkCmdNextSubpass2},
    {"vkCmdNextSubpass2KHR", vt_call_vkCmdNextSubpass2},
    {"vkCmdEndRenderPass2", vt_call_vkCmdEndRenderPass2},
    {"vkCmdEndRenderPass2KHR", vt_call_vkCmdEndRenderPass2},
    {"vkGetSemaphoreCounterValue", vt_call_vkGetSemaphoreCounterValue},
    {"vkGetSemaphoreCounterValueKHR", vt_call_vkGetSemaphoreCounterValue},
    {"vkWaitSemaphores", vt_call_vkWaitSemaphores},
    {"vkWaitSemaphoresKHR", vt_call_vkWaitSemaphores},
    {"vkSignalSemaphore", vt_call_vkSignalSemaphore},
    {"vkSignalSemaphoreKHR", vt_call_vkSignalSemaphore},
    {"vkCmdDrawIndirectCount", vt_call_vkCmdDrawIndirectCount},
    {"vkCmdDrawIndirectCountKHR", vt_call_vkCmdDrawIndirectCount},
    {"vkCmdDrawIndexedIndirectCount", vt_call_vkCmdDrawIndexedIndirectCount},
    {"vkCmdDrawIndexedIndirectCountKHR", vt_call_vkCmdDrawIndexedIndirectCount},
    {"vkCmdBindTransformFeedbackBuffersEXT", vt_call_vkCmdBindTransformFeedbackBuffersEXT},
    {"vkCmdBeginTransformFeedbackEXT", vt_call_vkCmdBeginTransformFeedbackEXT},
    {"vkCmdEndTransformFeedbackEXT", vt_call_vkCmdEndTransformFeedbackEXT},
    {"vkCmdBeginQueryIndexedEXT", vt_call_vkCmdBeginQueryIndexedEXT},
    {"vkCmdEndQueryIndexedEXT", vt_call_vkCmdEndQueryIndexedEXT},
    {"vkCmdDrawIndirectByteCountEXT", vt_call_vkCmdDrawIndirectByteCountEXT},
    {"vkGetBufferOpaqueCaptureAddress", vt_call_vkGetBufferOpaqueCaptureAddress},
    {"vkGetBufferDeviceAddress", vt_call_vkGetBufferDeviceAddress},
    {"vkGetDeviceMemoryOpaqueCaptureAddress", vt_call_vkGetDeviceMemoryOpaqueCaptureAddress},
    {"vkCmdSetLineStippleKHR", vt_call_vkCmdSetLineStippleKHR},
    {"vkCmdSetLineStippleEXT", vt_call_vkCmdSetLineStippleKHR},
    {"vkGetPhysicalDeviceToolProperties", vt_call_vkGetPhysicalDeviceToolProperties},
    {"vkCmdSetCullMode", vt_call_vkCmdSetCullMode},
    {"vkCmdSetCullModeEXT", vt_call_vkCmdSetCullMode},
    {"vkCmdSetFrontFace", vt_call_vkCmdSetFrontFace},
    {"vkCmdSetFrontFaceEXT", vt_call_vkCmdSetFrontFace},
    {"vkCmdSetPrimitiveTopology", vt_call_vkCmdSetPrimitiveTopology},
    {"vkCmdSetPrimitiveTopologyEXT", vt_call_vkCmdSetPrimitiveTopology},
    {"vkCmdSetViewportWithCount", vt_call_vkCmdSetViewportWithCount},
    {"vkCmdSetViewportWithCountEXT", vt_call_vkCmdSetViewportWithCount},
    {"vkCmdSetScissorWithCount", vt_call_vkCmdSetScissorWithCount},
    {"vkCmdSetScissorWithCountEXT", vt_call_vkCmdSetScissorWithCount},
    {"vkCmdBindVertexBuffers2", vt_call_vkCmdBindVertexBuffers2},
    {"vkCmdBindVertexBuffers2EXT", vt_call_vkCmdBindVertexBuffers2},
    {"vkCmdSetDepthTestEnable", vt_call_vkCmdSetDepthTestEnable},
    {"vkCmdSetDepthTestEnableEXT", vt_call_vkCmdSetDepthTestEnable},
    {"vkCmdSetDepthWriteEnable", vt_call_vkCmdSetDepthWriteEnable},
    {"vkCmdSetDepthWriteEnableEXT", vt_call_vkCmdSetDepthWriteEnable},
    {"vkCmdSetDepthCompareOp", vt_call_vkCmdSetDepthCompareOp},
    {"vkCmdSetDepthCompareOpEXT", vt_call_vkCmdSetDepthCompareOp},
    {"vkCmdSetDepthBoundsTestEnable", vt_call_vkCmdSetDepthBoundsTestEnable},
    {"vkCmdSetDepthBoundsTestEnableEXT", vt_call_vkCmdSetDepthBoundsTestEnable},
    {"vkCmdSetStencilTestEnable", vt_call_vkCmdSetStencilTestEnable},
    {"vkCmdSetStencilTestEnableEXT", vt_call_vkCmdSetStencilTestEnable},
    {"vkCmdSetStencilOp", vt_call_vkCmdSetStencilOp},
    {"vkCmdSetStencilOpEXT", vt_call_vkCmdSetStencilOp},
    {"vkCmdSetRasterizerDiscardEnable", vt_call_vkCmdSetRasterizerDiscardEnable},
    {"vkCmdSetDepthBiasEnable", vt_call_vkCmdSetDepthBiasEnable},
    {"vkCmdSetPrimitiveRestartEnable", vt_call_vkCmdSetPrimitiveRestartEnable},
    {"vkCmdSetTessellationDomainOriginEXT", vt_call_vkCmdSetTessellationDomainOriginEXT},
    {"vkCmdSetDepthClampEnableEXT", vt_call_vkCmdSetDepthClampEnableEXT},
    {"vkCmdSetPolygonModeEXT", vt_call_vkCmdSetPolygonModeEXT},
    {"vkCmdSetRasterizationSamplesEXT", vt_call_vkCmdSetRasterizationSamplesEXT},
    {"vkCmdSetSampleMaskEXT", vt_call_vkCmdSetSampleMaskEXT},
    {"vkCmdSetAlphaToCoverageEnableEXT", vt_call_vkCmdSetAlphaToCoverageEnableEXT},
    {"vkCmdSetAlphaToOneEnableEXT", vt_call_vkCmdSetAlphaToOneEnableEXT},
    {"vkCmdSetLogicOpEnableEXT", vt_call_vkCmdSetLogicOpEnableEXT},
    {"vkCmdSetColorBlendEnableEXT", vt_call_vkCmdSetColorBlendEnableEXT},
    {"vkCmdSetColorBlendEquationEXT", vt_call_vkCmdSetColorBlendEquationEXT},
    {"vkCmdSetColorWriteMaskEXT", vt_call_vkCmdSetColorWriteMaskEXT},
    {"vkCmdSetRasterizationStreamEXT", vt_call_vkCmdSetRasterizationStreamEXT},
    {"vkCmdSetConservativeRasterizationModeEXT", vt_call_vkCmdSetConservativeRasterizationModeEXT},
    {"vkCmdSetExtraPrimitiveOverestimationSizeEXT", vt_call_vkCmdSetExtraPrimitiveOverestimationSizeEXT},
    {"vkCmdSetDepthClipEnableEXT", vt_call_vkCmdSetDepthClipEnableEXT},
    {"vkCmdSetSampleLocationsEnableEXT", vt_call_vkCmdSetSampleLocationsEnableEXT},
    {"vkCmdSetColorBlendAdvancedEXT", vt_call_vkCmdSetColorBlendAdvancedEXT},
    {"vkCmdSetProvokingVertexModeEXT", vt_call_vkCmdSetProvokingVertexModeEXT},
    {"vkCmdSetLineRasterizationModeEXT", vt_call_vkCmdSetLineRasterizationModeEXT},
    {"vkCmdSetLineStippleEnableEXT", vt_call_vkCmdSetLineStippleEnableEXT},
    {"vkCmdSetDepthClipNegativeOneToOneEXT", vt_call_vkCmdSetDepthClipNegativeOneToOneEXT},
    {"vkCreatePrivateDataSlot", vt_call_vkCreatePrivateDataSlot},
    {"vkDestroyPrivateDataSlot", vt_call_vkDestroyPrivateDataSlot},
    {"vkSetPrivateData", vt_call_vkSetPrivateData},
    {"vkGetPrivateData", vt_call_vkGetPrivateData},
    {"vkCmdCopyBuffer2", vt_call_vkCmdCopyBuffer2},
    {"vkCmdCopyImage2", vt_call_vkCmdCopyImage2},
    {"vkCmdBlitImage2", vt_call_vkCmdBlitImage2},
    {"vkCmdCopyBufferToImage2", vt_call_vkCmdCopyBufferToImage2},
    {"vkCmdCopyImageToBuffer2", vt_call_vkCmdCopyImageToBuffer2},
    {"vkCmdResolveImage2", vt_call_vkCmdResolveImage2},
    {"vkCmdSetColorWriteEnableEXT", vt_call_vkCmdSetColorWriteEnableEXT},
    {"vkCmdSetEvent2", vt_call_vkCmdSetEvent2},
    {"vkCmdSetEvent2KHR", vt_call_vkCmdSetEvent2},
    {"vkCmdResetEvent2", vt_call_vkCmdResetEvent2},
    {"vkCmdResetEvent2KHR", vt_call_vkCmdResetEvent2},
    {"vkCmdWaitEvents2", vt_call_vkCmdWaitEvents2},
    {"vkCmdWaitEvents2KHR", vt_call_vkCmdWaitEvents2},
    {"vkCmdPipelineBarrier2", vt_call_vkCmdPipelineBarrier2},
    {"vkCmdPipelineBarrier2KHR", vt_call_vkCmdPipelineBarrier2},
    {"vkQueueSubmit2", vt_call_vkQueueSubmit2},
    {"vkQueueSubmit2KHR", vt_call_vkQueueSubmit2},
    {"vkCmdWriteTimestamp2", vt_call_vkCmdWriteTimestamp2},
    {"vkCmdWriteTimestamp2KHR", vt_call_vkCmdWriteTimestamp2},
    {"vkCmdBeginRendering", vt_call_vkCmdBeginRendering},
    {"vkCmdBeginRenderingKHR", vt_call_vkCmdBeginRendering},
    {"vkCmdEndRendering", vt_call_vkCmdEndRendering},
    {"vkCmdEndRenderingKHR", vt_call_vkCmdEndRendering},
    {"vkGetShaderModuleIdentifierEXT", vt_call_vkGetShaderModuleIdentifierEXT},
    {"vkGetShaderModuleCreateInfoIdentifierEXT", vt_call_vkGetShaderModuleCreateInfoIdentifierEXT},
    {"vkMapMemory2KHR", vt_call_vkMapMemory2KHR},
    {"vkUnmapMemory2KHR", vt_call_vkUnmapMemory2KHR},
};

static void* findVkDispatchFuncWithName(const char* name) {
    for (int i = 0; i < ARRAY_SIZE(vkDispatchTable); i++) {
        if (strcmp(name, vkDispatchTable[i].name) == 0) return vkDispatchTable[i].func;
    }
    return NULL;
}

PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (!vortekInitOnce()) return NULL;
    return findVkDispatchFuncWithName(pName);
}

VkResult vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* pSupportedVersion) {
    *pSupportedVersion = 3;
    return VK_SUCCESS;
}