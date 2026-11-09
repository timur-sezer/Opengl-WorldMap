#pragma once

// Downloads slippy-map tiles (z/x/y, 256x256) on background threads, caches them on disk,
// and turns them into OpenGL textures on the main (GL) thread.

#include "glad/glad.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct TileKey
{
    int z = 0, x = 0, y = 0;
    bool operator==(const TileKey& o) const { return z == o.z && x == o.x && y == o.y; }
};

struct TileKeyHash
{
    size_t operator()(const TileKey& k) const
    {
        return (size_t(k.z) << 58) ^ (size_t(k.x) << 29) ^ size_t(k.y);
    }
};

struct TileSource
{
    const char* id;           // used as the cache sub-folder
    const char* name;         // shown in the UI
    const char* urlTemplate;  // contains {z}, {x}, {y}
    const char* attribution;  // must be shown on the map
    int maxZoom;
};

class TileManager
{
public:
    TileManager(const TileSource& source, std::string cacheDir, int workerCount = 4);
    ~TileManager();
    TileManager(const TileManager&) = delete;
    TileManager& operator=(const TileManager&) = delete;

    // Call once per frame on the GL thread, before any getTexture() call.
    // Uploads finished tiles, drops requests nobody needs any more, evicts old textures.
    void beginFrame();

    // Returns the texture for a tile, or 0 if it is not loaded yet.
    // With request == true a missing tile is queued for download.
    GLuint getTexture(const TileKey& key, bool request);

    const TileSource& source() const { return source_; }

private:
    enum class State { Queued, Ready, Failed };
    struct Entry
    {
        State state = State::Queued;
        GLuint texture = 0;
        uint64_t lastUsedFrame = 0;
        double failedAt = 0.0;
    };
    struct Request { TileKey key; };
    struct Result
    {
        TileKey key;
        std::vector<unsigned char> pixels;  // RGBA8
        int width = 0, height = 0;
        bool ok = false;
    };

    void enqueue(const TileKey& key);
    void workerLoop();
    bool download(void* curl, const TileKey& key, std::vector<unsigned char>& bytes);
    std::string url(const TileKey& key) const;
    std::string cachePath(const TileKey& key) const;
    static int progressCallback(void* self, long long, long long, long long, long long);

    TileSource source_;
    std::string cacheDir_;

    // Main thread only
    std::unordered_map<TileKey, Entry, TileKeyHash> entries_;
    uint64_t frame_ = 0;

    // Shared with workers
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Request> requests_;  // used as a stack: newest requests are served first
    std::vector<Result> results_;
    std::atomic<bool> stopping_{false};
    std::vector<std::thread> workers_;
};
