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
NotifyFdPool notifyFdPool = {0};

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

    int fds[2 + MAX_NOTIFY_FDS];
    int numFds;
    recv_fds(serverFd, fds, &numFds, NULL, 0);
    if (numFds != ARRAY_SIZE(fds)) return false;

    serverRing = RingBuffer_create(fds[0], SERVER_RING_BUFFER_SIZE);
    if (!serverRing) return false;

    clientRing = RingBuffer_create(fds[1], CLIENT_RING_BUFFER_SIZE);
    if (!clientRing) return false;

    NotifyFdPool_create(&notifyFdPool, fds + 2);

    close(fds[0]);
    close(fds[1]);

    mp_init(&globalMemoryPool, MEMORY_POOL_MAX_SIZE);
    return true;
}

static void terminationCallback() {
    CLOSEFD(serverFd);
    if (serverRing) RingBuffer_free(serverRing);
    if (clientRing) RingBuffer_free(clientRing);

    NotifyFdPool_destroy(&notifyFdPool);
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