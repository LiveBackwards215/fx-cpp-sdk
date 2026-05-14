#pragma once

#include "Context.h"

#include <coroutine>
#include <memory>

namespace fxw_internal
{

struct WasmPromise;
using WasmCoroutineHandle = std::coroutine_handle<WasmPromise>;

struct WasmTask;

struct WasmPromise
{
    int64_t waitMs = 0;
    WasmTask get_return_object();
    std::suspend_always initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() {}
};

struct WasmTask
{
    using promise_type = WasmPromise;
    WasmCoroutineHandle handle;
};

inline WasmTask WasmPromise::get_return_object()
{
    return WasmTask{WasmCoroutineHandle::from_promise(*this)};
}

struct CoroutineEntry
{
    WasmCoroutineHandle handle;
    std::shared_ptr<void> prevent_destruct;
    std::chrono::steady_clock::time_point resumeAt;
};

inline std::unordered_map<uint64_t, CoroutineEntry>& coroutines()
{
    static std::unordered_map<uint64_t, CoroutineEntry> s_map;
    return s_map;
}

inline uint64_t& nextCoroutineId()
{
    static uint64_t s_id = 1;
    return s_id;
}

inline void resumeCoroutines()
{
    auto& coros = coroutines();
    if (coros.empty()) return;
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> ready;
    for (auto& [id, entry] : coros)
        if (now >= entry.resumeAt) ready.push_back(id);
    for (auto id : ready)
    {
        auto it = coros.find(id);
        if (it == coros.end()) continue;
        auto handle = it->second.handle;
        handle.promise().waitMs = 0;
        handle.resume();
        if (handle.done())
        {
            handle.destroy();
            coros.erase(it);
        }
        else
        {
            int64_t waitMs = handle.promise().waitMs;
            it->second.resumeAt = (waitMs > 0) ? now + std::chrono::milliseconds(waitMs) : now;
        }
    }
}

inline void cleanupCoroutines()
{
    auto& coros = coroutines();
    for (auto& [id, entry] : coros)
    {
        if (entry.handle) entry.handle.destroy();
    }
    coros.clear();
}

}

namespace fx
{

using ScriptTask = fxw_internal::WasmTask;

struct Wait
{
    int64_t ms;
    explicit Wait(int64_t ms) : ms(ms) {}
    bool await_ready() const noexcept { return false; }
    void await_suspend(fxw_internal::WasmCoroutineHandle h) const noexcept
    {
        h.promise().waitMs = ms;
    }
    void await_resume() const noexcept {}
};

template<typename F>
inline void createThread(F&& fn)
{
    auto* c = fxw_internal::currentContext();
    if (!c) return;
    auto& coros = fxw_internal::coroutines();
    if (coros.size() >= 4096) return;
    auto stored = std::make_shared<std::decay_t<F>>(std::forward<F>(fn));
    auto task = (*stored)();
    uint64_t id = fxw_internal::nextCoroutineId()++;
    coros[id] = { task.handle, std::move(stored), std::chrono::steady_clock::now() };
}

}
