#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/mman.h>

#include "vortek.h"

int serverFd = -1;
uint16_t maxClientRequestId = 1;
MemoryPool globalMemoryPool = {0};
RingBuffer* serverRing = NULL;
RingBuffer* clientRing = NULL;
RingBufferPool globalRingPool[RING_POOL_ARRAY_LEN] = {0};

static int vortekServerConnect() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_LOCAL;

    strncpy(server_addr.sun_path, VORTEK_SERVER_PATH, sizeof(server_addr.sun_path) - 1);

    int res;
    do {
        res = 0;
        if (connect(fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_un)) < 0) res = -errno;
    }
    while (res == -EINTR);

    if (res < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static bool createVkContext() {
    char header[HEADER_SIZE];
    *(int*)(header + 0) = REQUEST_CODE_CREATE_CONTEXT;
    *(int*)(header + 4) = 0;

    int res = write(serverFd, header, HEADER_SIZE);
    if (res < 0) return false;

    int shmFds[2 + RING_POOL_ARRAY_LEN];
    int numFds;
    recv_fds(serverFd, shmFds, &numFds, NULL, 0);
    if (numFds != ARRAY_SIZE(shmFds)) return false;

    serverRing = RingBuffer_create(shmFds[0], SERVER_RING_BUFFER_SIZE);
    if (!serverRing) return false;

    clientRing = RingBuffer_create(shmFds[1], CLIENT_RING_BUFFER_SIZE);
    if (!clientRing) return false;

    for (int i = 0; i < RING_POOL_ARRAY_LEN; i++) {
        globalRingPool[i].ring = RingBuffer_create(shmFds[2+i], RING_POOL_BUFFER_SIZE);
        globalRingPool[i].ring->useSemaphores = true;
        globalRingPool[i].acquired = false;
        if (!globalRingPool[i].ring) return false;
    }

    for (int i = 0; i < ARRAY_SIZE(shmFds); i++) close(shmFds[i]);

    mp_init(&globalMemoryPool, MEMORY_POOL_MAX_SIZE);
    return true;
}

static void terminationCallback() {
    CLOSEFD(serverFd);
    if (serverRing) RingBuffer_free(serverRing);
    if (clientRing) RingBuffer_free(clientRing);

    for (int i = 0; i < RING_POOL_ARRAY_LEN; i++) {
        if (globalRingPool[i].ring) RingBuffer_free(globalRingPool[i].ring);
    }

    mp_free(&globalMemoryPool);
    MEMFREE(globalMemoryPool.data);
}

bool vortekInitOnce() {
    if (serverFd == -1) {
        serverFd = vortekServerConnect();

        if (serverFd > 0) {
            if (!createVkContext()) return false;
#if DEBUG_MODE
            println("vortek: connected serverFd=%d pid=%d\n", serverFd, getpid());
#endif
        }

        atexit(terminationCallback);
    }

    return serverFd > 0;
}