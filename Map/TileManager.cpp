#include "TileManager.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
constexpr size_t kMaxTextures = 600;     // ~150 MB of RGBA tiles at most
constexpr uint64_t kStaleFrames = 30;    // queued tiles off-screen this long are cancelled
constexpr double kRetrySeconds = 10.0;   // wait before retrying a failed tile

double nowSeconds()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* out = static_cast<std::vector<unsigned char>*>(userdata);
    out->insert(out->end(), ptr, ptr + size * nmemb);
    return size * nmemb;
}

void replaceAll(std::string& s, const std::string& from, const std::string& to)
{
    for (size_t pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size()))
        s.replace(pos, from.size(), to);
}

bool readFile(const std::string& path, std::vector<unsigned char>& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !out.empty();
}

void writeFile(const std::string& path, const std::vector<unsigned char>& data)
{
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}
} // namespace

TileManager::TileManager(const TileSource& source, std::string cacheDir, int workerCount)
    : source_(source), cacheDir_(std::move(cacheDir))
{
    static std::once_flag curlInit;
    std::call_once(curlInit, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    for (int i = 0; i < workerCount; ++i)
        workers_.emplace_back(&TileManager::workerLoop, this);
}

TileManager::~TileManager()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_)
        t.join();

    for (auto& [key, entry] : entries_)
        if (entry.texture)
            glDeleteTextures(1, &entry.texture);
}

void TileManager::beginFrame()
{
    ++frame_;

    // 1. Upload tiles the workers finished.
    std::vector<Result> done;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        done.swap(results_);

        // Cancel queued requests for tiles that scrolled off-screen a while ago.
        requests_.erase(std::remove_if(requests_.begin(), requests_.end(), [&](const Request& r) {
            auto it = entries_.find(r.key);
            if (it == entries_.end() || it->second.lastUsedFrame + kStaleFrames < frame_)
            {
                if (it != entries_.end())
                    entries_.erase(it);
                return true;
            }
            return false;
        }), requests_.end());
    }

    for (Result& r : done)
    {
        auto it = entries_.find(r.key);
        if (it == entries_.end())
            continue;
        Entry& e = it->second;
        if (!r.ok)
        {
            e.state = State::Failed;
            e.failedAt = nowSeconds();
            continue;
        }
        glGenTextures(1, &e.texture);
        glBindTexture(GL_TEXTURE_2D, e.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r.width, r.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, r.pixels.data());
        e.state = State::Ready;
    }

    // 2. Evict least recently used textures (never ones drawn last frame).
    size_t ready = 0;
    for (auto& [key, e] : entries_)
        ready += e.state == State::Ready;
    if (ready <= kMaxTextures)
        return;

    std::vector<std::pair<uint64_t, TileKey>> candidates;
    for (auto& [key, e] : entries_)
        if (e.state == State::Ready && e.lastUsedFrame + 1 < frame_)
            candidates.push_back({e.lastUsedFrame, key});
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (auto& [used, key] : candidates)
    {
        if (ready <= kMaxTextures)
            break;
        auto it = entries_.find(key);
        glDeleteTextures(1, &it->second.texture);
        entries_.erase(it);
        --ready;
    }
}

GLuint TileManager::getTexture(const TileKey& key, bool request)
{
    auto it = entries_.find(key);
    if (it == entries_.end())
    {
        if (request)
        {
            Entry e;
            e.lastUsedFrame = frame_;
            entries_.emplace(key, e);
            enqueue(key);
        }
        return 0;
    }

    Entry& e = it->second;
    e.lastUsedFrame = frame_;
    if (e.state == State::Ready)
        return e.texture;
    if (e.state == State::Failed && request && nowSeconds() - e.failedAt > kRetrySeconds)
    {
        e.state = State::Queued;
        enqueue(key);
    }
    return 0;
}

void TileManager::enqueue(const TileKey& key)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requests_.push_back({key});
    }
    cv_.notify_one();
}

void TileManager::workerLoop()
{
    CURL* curl = curl_easy_init();
    std::vector<unsigned char> bytes;

    while (true)
    {
        Request req;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { return stopping_ || !requests_.empty(); });
            if (stopping_)
                break;
            req = requests_.back();
            requests_.pop_back();
        }

        Result res;
        res.key = req.key;
        bytes.clear();

        const std::string path = cachePath(req.key);
        const bool fromCache = readFile(path, bytes);
        if (!fromCache && !download(curl, req.key, bytes))
            bytes.clear();

        if (!bytes.empty())
        {
            int channels = 0;
            unsigned char* px = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                      &res.width, &res.height, &channels, 4);
            if (px)
            {
                res.pixels.assign(px, px + size_t(res.width) * res.height * 4);
                stbi_image_free(px);
                res.ok = true;
                if (!fromCache)
                    writeFile(path, bytes);
            }
            else if (fromCache)
            {
                std::error_code ec;
                fs::remove(path, ec);  // corrupt cache entry
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        results_.push_back(std::move(res));
    }

    curl_easy_cleanup(curl);
}

int TileManager::progressCallback(void* self, long long, long long, long long, long long)
{
    // Returning non-zero aborts the transfer, so shutdown never waits for a slow download.
    return static_cast<TileManager*>(self)->stopping_ ? 1 : 0;
}

bool TileManager::download(void* handle, const TileKey& key, std::vector<unsigned char>& bytes)
{
    CURL* curl = static_cast<CURL*>(handle);
    const std::string u = url(key);

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, u.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Opengl-WorldMap/0.1");  // tile servers require a real User-Agent
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

    const CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK)
    {
        static std::atomic<int> reported{0};
        if (!stopping_ && reported++ < 5)
            fprintf(stderr, "Opengl-WorldMap: tile %s failed: %s\n", u.c_str(), curl_easy_strerror(rc));
        return false;
    }
    return true;
}

std::string TileManager::url(const TileKey& key) const
{
    std::string u = source_.urlTemplate;
    replaceAll(u, "{z}", std::to_string(key.z));
    replaceAll(u, "{x}", std::to_string(key.x));
    replaceAll(u, "{y}", std::to_string(key.y));
    return u;
}

std::string TileManager::cachePath(const TileKey& key) const
{
    return cacheDir_ + "/" + std::to_string(key.z) + "/" + std::to_string(key.x) + "/" + std::to_string(key.y) + ".tile";
}
