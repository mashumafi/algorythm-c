#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include <httplib.h>
#include <cJSON.h>

#include <simpleble/SimpleBLE.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

// Simple shared audio context for device enumeration and ID retention.
static std::mutex g_audioMutex;
static ma_context g_ctx;
static bool g_ctx_inited = false;
static int g_selectedPlaybackIndex = -1;

static void ensure_audio_context() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_ctx_inited) {
        if (ma_context_init(nullptr, 0, nullptr, &g_ctx) != MA_SUCCESS) {
            // Failed; leave uninitialized
            return;
        }
        g_ctx_inited = true;
    }
}

static std::string render_ble_list() {
    using namespace SimpleBLE;
    std::string html;
    html += "<table><thead><tr><th>Name</th><th>Address</th><th>Status</th><th>Action</th></tr></thead><tbody>";
    try {
        auto adapters = Adapter::get_adapters();
        if (adapters.empty()) {
            html += "<tr><td colspan=4>No adapters found</td></tr>";
        } else {
            auto adapter = adapters.front();
            adapter.scan_for(1500);
            auto peripherals = adapter.scan_get_results();
            if (peripherals.empty()) {
                html += "<tr><td colspan=4>No devices found</td></tr>";
            } else {
                for (auto& p : peripherals) {
                    auto name = p.identifier();
                    auto addr = p.address();
                    bool connected = false;
                    try { connected = p.is_connected(); } catch(...) { connected = false; }
                    html += "<tr>";
                    html += std::string("<td>") + (name.empty()?"&lt;unknown&gt;":name) + "</td>";
                    html += std::string("<td>") + addr + "</td>";
                    html += std::string("<td>") + (connected?"connected":"disconnected") + "</td>";
                    html += std::string("<td><button hx-post=\"/ble/toggle?address=") + addr + "\" hx-target=\"#ble-list\" hx-swap=\"outerHTML\">Toggle</button></td>";
                    html += "</tr>";
                }
            }
        }
    } catch(...) {
        html += "<tr><td colspan=4>Error scanning BLE</td></tr>";
    }
    html += "</tbody></table>";
    return std::string("<div id=\"ble-list\">") + html + "</div>";
}

static std::string render_audio_list() {
    ensure_audio_context();
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_ctx_inited) {
        return "<div id=\"audio-list\"><em>Audio context init failed</em></div>";
    }

    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    ma_device_info* pCaptureInfos = nullptr;
    ma_uint32 captureCount = 0;

    if (ma_context_get_devices(&g_ctx, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        return "<div id=\"audio-list\"><em>Failed to enumerate devices</em></div>";
    }

    std::string html;
    html += "<ul>";
    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        const char* name = pPlaybackInfos[i].name ? pPlaybackInfos[i].name : "<unknown>";
        bool active = ((int)i == g_selectedPlaybackIndex);
        html += std::string("<li>") + (active ? "<strong>" : "") + name + (active ? "</strong>" : "");
        html += std::string(" <button hx-post=\"/audio/select?index=") + std::to_string(i) + "\" hx-target=\"#audio-list\" hx-swap=\"outerHTML\">Select</button>";
        html += "</li>";
    }
    html += "</ul>";
    return std::string("<div id=\"audio-list\">") + html + "</div>";
}

struct NoiseState {
    float amplitude;
    ma_uint32 channels;
    unsigned int seed;
};

static inline float frand_signed(unsigned int* s) {
    *s = (*s * 1664525u) + 1013904223u;
    float v = (float)(*s & 0x00FFFFFF) / (float)0x01000000;
    return (v * 2.0f) - 1.0f;
}

static void data_callback(ma_device* device, void* out, const void* in, ma_uint32 frameCount) {
    NoiseState* st = (NoiseState*)device->pUserData;
    float* f32 = (float*)out;
    ma_uint64 total = (ma_uint64)frameCount * st->channels;
    for (ma_uint64 i = 0; i < total; ++i) {
        f32[i] = frand_signed(&st->seed) * st->amplitude;
    }
    (void)in;
}

// Persistent noise device to avoid spawning a thread per request.
static ma_device g_noiseDevice;
static bool g_noiseDeviceInited = false;
static bool g_noiseRunning = false;
static NoiseState g_noiseState{};
static std::thread g_noiseMonitor;
static std::atomic<bool> g_monitorRunning{false};
static bool g_hasDeadline = false;
static std::chrono::steady_clock::time_point g_noiseStopAt;

static void start_monitor_if_needed() {
    if (!g_monitorRunning.load()) {
        g_monitorRunning.store(true);
        g_noiseMonitor = std::thread([]() {
            while (g_monitorRunning.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::lock_guard<std::mutex> lock(g_audioMutex);
                if (g_noiseRunning && g_hasDeadline) {
                    if (std::chrono::steady_clock::now() >= g_noiseStopAt) {
                        ma_device_stop(&g_noiseDevice);
                        g_noiseRunning = false;
                        if (g_noiseDeviceInited) {
                            ma_device_uninit(&g_noiseDevice);
                            g_noiseDeviceInited = false;
                        }
                        g_hasDeadline = false;
                    }
                }
            }
        });
    }
}

static bool start_noise(ma_uint32 rate, ma_uint32 channels, float amp, ma_uint32 duration_ms) {
    ensure_audio_context();
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (!g_ctx_inited) return false;

    // If already running, stop and uninit so we can reconfigure.
    if (g_noiseRunning) {
        ma_device_stop(&g_noiseDevice);
        g_noiseRunning = false;
    }
    if (g_noiseDeviceInited) {
        ma_device_uninit(&g_noiseDevice);
        g_noiseDeviceInited = false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = channels;
    config.sampleRate = rate;
    config.dataCallback = data_callback;

    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(&g_ctx, &pPlaybackInfos, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
        if (g_selectedPlaybackIndex >= 0 && (ma_uint32)g_selectedPlaybackIndex < playbackCount) {
            config.playback.pDeviceID = &pPlaybackInfos[g_selectedPlaybackIndex].id;
        }
    }

    g_noiseState.amplitude = amp;
    g_noiseState.channels = channels;
    g_noiseState.seed = 1234567u;
    config.pUserData = &g_noiseState;

    if (ma_device_init(&g_ctx, &config, &g_noiseDevice) != MA_SUCCESS) {
        return false;
    }
    g_noiseDeviceInited = true;
    if (ma_device_start(&g_noiseDevice) != MA_SUCCESS) {
        ma_device_uninit(&g_noiseDevice);
        g_noiseDeviceInited = false;
        return false;
    }
    g_noiseRunning = true;
    g_hasDeadline = duration_ms > 0;
    if (g_hasDeadline) {
        g_noiseStopAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        start_monitor_if_needed();
    }
    return true;
}

static void stop_noise() {
    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (g_noiseRunning) {
        ma_device_stop(&g_noiseDevice);
        g_noiseRunning = false;
    }
    if (g_noiseDeviceInited) {
        ma_device_uninit(&g_noiseDevice);
        g_noiseDeviceInited = false;
    }
    g_hasDeadline = false;
}

int main() {
    ensure_audio_context();

    httplib::Server svr;

    // Serve static assets placed in build root: build/static_html
    svr.set_mount_point("/", "static_html");
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/index.html");
    });

    // BLE list and toggle
    svr.Get("/ble/list", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(render_ble_list(), "text/html; charset=utf-8");
    });

    svr.Post("/ble/toggle", [](const httplib::Request& req, httplib::Response& res) {
        using namespace SimpleBLE;
        std::string address = req.get_param_value("address");
        try {
            auto adapters = Adapter::get_adapters();
            if (!adapters.empty()) {
                auto adapter = adapters.front();
                adapter.scan_for(1500);
                auto results = adapter.scan_get_results();
                for (auto& p : results) {
                    if (p.address() == address) {
                        bool connected = false;
                        try { connected = p.is_connected(); } catch(...) { connected = false; }
                        if (connected) {
                            try { p.disconnect(); } catch(...) {}
                        } else {
                            try { p.connect(); } catch(...) {}
                        }
                        break;
                    }
                }
            }
        } catch(...) {
            // ignore errors
        }
        // Return updated list HTML
        res.set_content(render_ble_list(), "text/html; charset=utf-8");
    });

    // Audio devices list and selection
    svr.Get("/audio/list", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(render_audio_list(), "text/html; charset=utf-8");
    });

    svr.Post("/audio/select", [](const httplib::Request& req, httplib::Response& res) {
        int idx = -1;
        try { idx = std::stoi(req.get_param_value("index")); } catch(...) { idx = -1; }
        {
            std::lock_guard<std::mutex> lock(g_audioMutex);
            g_selectedPlaybackIndex = idx;
        }
        res.set_content(render_audio_list(), "text/html; charset=utf-8");
    });

    // White noise via JSON body
    svr.Post("/audio/whitenoise", [](const httplib::Request& req, httplib::Response& res) {
        ma_uint32 rate = 48000;
        ma_uint32 channels = 2;
        ma_uint32 duration_ms = 3000;
        float amp = 0.2f;
        if (!req.body.empty()) {
            cJSON* root = cJSON_Parse(req.body.c_str());
            if (root) {
                cJSON* jrate = cJSON_GetObjectItemCaseSensitive(root, "rate");
                cJSON* jch = cJSON_GetObjectItemCaseSensitive(root, "channels");
                cJSON* jdur = cJSON_GetObjectItemCaseSensitive(root, "duration_ms");
                cJSON* jamp = cJSON_GetObjectItemCaseSensitive(root, "amp");
                if (cJSON_IsNumber(jrate)) rate = (ma_uint32)jrate->valuedouble;
                if (cJSON_IsNumber(jch)) channels = (ma_uint32)jch->valuedouble;
                if (cJSON_IsNumber(jdur)) duration_ms = (ma_uint32)jdur->valuedouble;
                if (cJSON_IsNumber(jamp)) amp = (float)jamp->valuedouble;
                cJSON_Delete(root);
            }
        }
        if (channels == 0 || channels > 8) channels = 2;
        if (rate < 8000) rate = 8000;
        if (amp < 0.0f) amp = 0.0f;
        if (amp > 1.0f) amp = 1.0f;
        if (duration_ms < 100) duration_ms = 100;
        bool ok = start_noise(rate, channels, amp, duration_ms);
        res.set_content(ok ? (std::string("<small>White noise started for ") + std::to_string(duration_ms) + " ms</small>") : "<small>Failed to start noise.</small>", "text/html; charset=utf-8");
    });

    // Stop white noise
    svr.Post("/audio/whitenoise/stop", [](const httplib::Request&, httplib::Response& res) {
        stop_noise();
        res.set_content("<small>White noise stopped.</small>", "text/html; charset=utf-8");
    });

    const char* host = "127.0.0.1";
    int port = 8080;
    printf("Server listening at http://%s:%d\n", host, port);
    svr.listen(host, port);

    // Cleanup context on exit
    if (g_ctx_inited) {
        stop_noise();
        ma_context_uninit(&g_ctx);
        g_ctx_inited = false;
    }
    if (g_monitorRunning.load()) {
        g_monitorRunning.store(false);
        if (g_noiseMonitor.joinable()) g_noiseMonitor.join();
    }
    return 0;
}
