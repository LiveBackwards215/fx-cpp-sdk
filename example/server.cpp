#include <resource_sdk/SDK.h>
#include <memory>
#include <unordered_map>

static std::string extractId(const std::string& source)
{
    auto pos = source.find(':');
    return pos != std::string::npos ? source.substr(pos + 1) : source;
}

FXCPP_RESOURCE
{
    auto pending = std::make_shared<std::unordered_map<std::string, std::string>>();
    auto players = std::make_shared<std::unordered_map<std::string, std::string>>();

    fx::trace("Resource started.\n");

    fx::on("playerConnecting", [pending](const std::string& source, fx::EventArgs args) {
        std::string name = args.get<std::string>(0);
        (*pending)[extractId(source)] = name;
    });

    fx::on("playerJoining", [pending, players](const std::string& source, fx::EventArgs args) {
        std::string oldId = args.get<std::string>(0);
        auto it = pending->find(oldId);
        std::string name = (it != pending->end()) ? it->second : "Unknown";
        if (it != pending->end()) pending->erase(it);
        std::string id = extractId(source);
        (*players)[id] = name;
        fx::trace("%s (%s) has connected. Players Online: %zu\n", name.c_str(), id.c_str(), players->size());
    });

    fx::on("playerDropped", [players](const std::string& source, fx::EventArgs args) {
        std::string reason = args.get<std::string>(0);
        std::string id = extractId(source);
        std::string name = players->count(id) ? (*players)[id] : "Unknown";
        players->erase(id);
        fx::trace("%s (%s) has disconnected (%s). Players Online: %zu\n", name.c_str(), id.c_str(), reason.c_str(), players->size());
    });

    fx::on("chatMessage", [players](const std::string& source, fx::EventArgs args) {
        std::string name = args.size() > 1 ? args.get<std::string>(1) : "Unknown";
        std::string message = args.size() > 2 ? args.get<std::string>(2) : "";
        std::string id = extractId(source);
        fx::trace("%s (%s) has sent a chat message: %s\n", name.c_str(), id.c_str(), message.c_str());
    });

    fx::onCommand("players", [players](const std::string&, const std::vector<std::string>&) {
        fx::trace("--- Online Players (%zu) ---\n", players->size());
        for (const auto& [src, name] : *players)
            fx::trace("  [%s] %s\n", extractId(src).c_str(), name.c_str());
        fx::trace("----------------------------\n");
    });

    fx::addExport("getPlayerCount", [players](fx::EventArgs) -> fx::json::Value {
        fx::json::Value v;
        v.kind = fx::json::Value::Kind::Number;
        v.scalar = std::to_string(players->size());
        return v;
    });

    fx::addExport("getPlayerName", [players](fx::EventArgs args) -> fx::json::Value {
        std::string src = args.get<std::string>(0);
        fx::json::Value v;
        auto it = players->find(src);
        if (it != players->end())
        {
            v.kind = fx::json::Value::Kind::String;
            v.scalar = it->second;
        }
        return v;
    });
}
