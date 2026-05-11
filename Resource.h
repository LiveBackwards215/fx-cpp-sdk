#pragma once

#include "Connection.h"
#include "Json.h"

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <unistd.h>

#if defined(_WIN32)
#define FXCPP_EXPORT __declspec(dllexport)
#else
#define FXCPP_EXPORT __attribute__((visibility("default")))
#endif

namespace fx
{

class EventArgs
{
public:
    explicit EventArgs(const json::Value& arr) : m_arr(arr) {}

    size_t size() const { return m_arr.size(); }

    template<typename T> T get(size_t i) const;

    std::string str(size_t i) const { return m_arr.at(i).asStr(); }
    int integer(size_t i) const { return m_arr.at(i).asInt(); }
    double number(size_t i) const { return m_arr.at(i).asNum(); }
    bool boolean(size_t i) const { return m_arr.at(i).asBool(); }
    bool isNull(size_t i) const { return m_arr.at(i).isNull(); }

private:
    const json::Value& m_arr;
};

template<> inline std::string EventArgs::get<std::string>(size_t i) const { return str(i); }
template<> inline int EventArgs::get<int>(size_t i) const { return integer(i); }
template<> inline double EventArgs::get<double>(size_t i) const { return number(i); }
template<> inline float EventArgs::get<float>(size_t i) const { return static_cast<float>(number(i)); }
template<> inline bool EventArgs::get<bool>(size_t i) const { return boolean(i); }

using EventHandler = std::function<void(const std::string& source, EventArgs)>;
using TickHandler = std::function<void()>;
using CommandHandler = std::function<void(const std::string& source, const std::vector<std::string>& args)>;

class ResourceContext
{
public:
    ResourceContext(Connection& conn, std::string name) : m_conn(conn), m_name(std::move(name)) {}

    void on(const std::string& event, EventHandler h)
    {
        bool first = m_eventHandlers.find(event) == m_eventHandlers.end();
        m_eventHandlers[event].push_back(std::move(h));

        if (first)
        {
            m_conn.send(json::JsonObj().set("t", "sub").set("event", event).build());
        }
    }

    void onTick(TickHandler h)
    {
        m_tickHandlers.push_back(std::move(h));
    }

    void trace(const char* fmt, ...)
    {
        char buf[4096];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        m_conn.send(json::JsonObj().set("t", "trace").set("msg", buf).build());
    }

    void emit(const std::string& event, const std::vector<std::string>& rawArgs = {})
    {
        std::string argsJson = json::arrayOf(rawArgs);
        m_conn.send(json::JsonObj().set("t", "emit").set("event", event).setRaw("args", argsJson).build());
    }

    void emitNet(const std::string& event, int target, const std::vector<std::string>& rawArgs = {})
    {
        std::string argsJson = json::arrayOf(rawArgs);
        m_conn.send(json::JsonObj().set("t", "emitNet").set("event", event).set("target", target).setRaw("args", argsJson).build());
    }

    void onCommand(const std::string& command, CommandHandler h)
    {
        m_commandHandlers[command].push_back(std::move(h));
        m_conn.send(json::JsonObj().set("t", "registerCommand").set("command", command).build());
    }

    void dispatchCommand(const std::string& command, const std::string& source, const std::vector<std::string>& args)
    {
        auto it = m_commandHandlers.find(command);
        if (it == m_commandHandlers.end()) return;
        for (auto& h : it->second) h(source, args);
    }

    Connection& connection() { return m_conn; }
    const std::string& resourceName() const { return m_name; }

    void dispatchTick()
    {
        for (auto& h : m_tickHandlers) h();
    }

    void dispatchEvent(const std::string& name, const json::Value& args, const std::string& source)
    {
        auto it = m_eventHandlers.find(name);
        if (it == m_eventHandlers.end()) return;
        EventArgs ea(args);
        for (auto& h : it->second) h(source, ea);
    }

    using NativeCallback = std::function<void(const json::Value&)>;
    std::unordered_map<int, NativeCallback> pendingNatives;
    int nextNativeId = 1;

private:
    Connection& m_conn;
    std::string m_name;
    std::unordered_map<std::string, std::vector<EventHandler>> m_eventHandlers;
    std::unordered_map<std::string, std::vector<CommandHandler>> m_commandHandlers;
    std::vector<TickHandler> m_tickHandlers;
};

namespace detail
{
    inline ResourceContext* g_ctx = nullptr;
}

inline ResourceContext* GetContext() { return detail::g_ctx; }

inline void RunLoop(ResourceContext& ctx)
{
    auto& conn = ctx.connection();
    std::string json;

    while (conn.receive(json))
    {
        fx::json::Value msg;
        try { msg = fx::json::parse(json); }
        catch (...) { continue; }

        const std::string t = msg["t"].asStr();

        if (t == "tick")
        {
            ctx.dispatchTick();
        }
        else if (t == "event")
        {
            ctx.dispatchEvent(msg["event"].asStr(), msg["args"], msg["source"].asStr("-1"));
        }
        else if (t == "nr")
        {
            int id = msg["id"].asInt();
            auto it = ctx.pendingNatives.find(id);
            if (it != ctx.pendingNatives.end())
            {
                it->second(msg["r"]);
                ctx.pendingNatives.erase(it);
            }
        }
        else if (t == "cmd")
        {
            std::vector<std::string> args;
            for (size_t i = 0; i < msg["args"].size(); ++i)
                args.push_back(msg["args"].at(i).asStr());
            ctx.dispatchCommand(msg["command"].asStr(), msg["source"].asStr("-1"), args);
        }
    }
}

}

#define CPP_SDK(resource_name) \
    static void main_impl(fx::ResourceContext& ctx); \
    int main(int argc, char* argv[]) \
    { \
        const char* portEnv = std::getenv("FXCPP_PORT"); \
        uint16_t port = portEnv ? static_cast<uint16_t>(std::atoi(portEnv)) : 30698; \
        fx::Connection conn; \
        { \
            bool ok = false; \
            for (int _attempt = 0; _attempt < 20; ++_attempt) { \
                if (conn.connect("127.0.0.1", port)) { ok = true; break; } \
                fprintf(stderr, "[fivem-cpp-sdk] Waiting for bridge on port %u...\n", port); \
                usleep(500000); \
            } \
            if (!ok) { \
                fprintf(stderr, "[fivem-cpp-sdk] Could not connect to bridge.\nIs start fivem-cpp-sdk' in your server.cfg?\n"); \
                return 1; \
            } \
        } \
        fx::ResourceContext ctx(conn, resource_name); \
        fx::detail::g_ctx = &ctx; \
        conn.send(fx::json::JsonObj().set("t", "hello").set("resource", resource_name).build()); \
        { \
            std::string ack; \
            if (!conn.receive(ack)) { fprintf(stderr, "[fivem-cpp-sdk] No ack.\n"); return 1; } \
        } \
        main_impl(ctx); \
        fx::RunLoop(ctx); \
        return 0; \
    } \
    static void main_impl([[maybe_unused]] fx::ResourceContext& ctx)
