#pragma once

#include "Resource.h"
#include "Json.h"

#include <string>
#include <functional>
#include <cstdint>
#include <cstdio>

namespace fx::natives
{

namespace detail
{
    inline void appendArg(std::vector<std::string>& args, const char* v) { args.push_back(json::quote(v)) }
    inline void appendArg(std::vector<std::string>& args, std::string v) { args.push_back(json::quote(v)) }
    inline void appendArg(std::vector<std::string>& args, int v) { args.push_back(std::to_string(v)) }
    inline void appendArg(std::vector<std::string>& args, uint32_t v) { args.push_back(std::to_string(v)) }
    inline void appendArg(std::vector<std::string>& args, int64_t v) { args.push_back(std::to_string(v)) }
    inline void appendArg(std::vector<std::string>& args, float v) { char b[32]; snprintf(b,32,"%g",v); args.push_back(b) }
    inline void appendArg(std::vector<std::string>& args, double v) { char b[32]; snprintf(b,32,"%g",v); args.push_back(b) }
    inline void appendArg(std::vector<std::string>& args, bool v) { args.push_back(v ? "true" : "false") }
}

template<typename... TArgs>
inline json::Value invokeRaw(uint64_t hash, TArgs&&... args)
{
    auto* ctx = fx::detail::g_ctx;
    if (!ctx) return {};

    int id = ctx->nextNativeId++;

    std::vector<std::string> argList;
    (detail::appendArg(argList, std::forward<TArgs>(args)), ...);

    char hashStr[32];
    snprintf(hashStr, sizeof(hashStr), "0x%llX", static_cast<unsigned long long>(hash));

    ctx->connection().send(json::JsonObj().set("t", "native").set("id", id).set("hash", hashStr).setRaw("args", json::arrayOf(argList)).build());

    json::Value result;
    bool got = false;

    ctx->pendingNatives[id] = [&](const json::Value& v)
    {
        result = v;
        got = true;
    };

    auto& conn = ctx->connection();
    std::string incoming;

    while (!got && conn.receive(incoming))
    {
        json::Value msg;
        try { msg = json::parse(incoming) } catch (...) { continue }

        const std::string t = msg["t"].asStr();

        if (t == "nr")
        {
            int rid = msg["id"].asInt();
            auto it = ctx->pendingNatives.find(rid);
            if (it != ctx->pendingNatives.end())
            {
                it->second(msg["r"]);
                ctx->pendingNatives.erase(it);
            }
        }
        else if (t == "tick") { ctx->dispatchTick() }
        else if (t == "event")
        {
            ctx->dispatchEvent(msg["event"].asStr(), msg["args"], msg["source"].asStr("-1"));
        }
    }

    return result;
}

template<typename TResult = void, typename... TArgs>
inline TResult invoke(uint64_t hash, TArgs&&... args)
{
    if constexpr (std::is_same_v<TResult, void>)
    {
        invokeRaw(hash, std::forward<TArgs>(args)...);
    }
    else if constexpr (std::is_same_v<TResult, std::string>)
    {
        return invokeRaw(hash, std::forward<TArgs>(args)...).asStr();
    }
    else if constexpr (std::is_same_v<TResult, bool>)
    {
        return invokeRaw(hash, std::forward<TArgs>(args)...).asBool();
    }
    else if constexpr (std::is_floating_point_v<TResult>)
    {
        return static_cast<TResult>(invokeRaw(hash, std::forward<TArgs>(args)...).asNum());
    }
    else
    {
        return static_cast<TResult>(invokeRaw(hash, std::forward<TArgs>(args)...).asNum());
    }
}

}
