#include <resource_sdk/SDK.h>
#include <memory>
#include <unordered_map>

FXCPP_RESOURCE
{
    auto pending = std::make_shared<std::unordered_map<std::string, std::string>>();
    auto players = std::make_shared<std::unordered_map<std::string, std::string>>();

    fx::trace("Resource started.\n");

    fx::on("playerConnecting", [pending](const std::string& source, fx::EventArgs args)
    {
        (*pending)[source] = args.get<std::string>(0);
    });

    fx::on("playerJoining", [pending, players](const std::string& source, fx::EventArgs args)
    {
        const std::string oldId = args.get<std::string>(0);
        auto it = pending->find(oldId);
        if (it != pending->end())
        {
            (*players)[source] = it->second;
            pending->erase(it);
        }
        const std::string name = players->count(source) ? (*players)[source] : source;
        fx::trace("%s (id: %s) joined. Players online: %zu\n", name.c_str(), source.c_str(), players->size());
    });

    fx::on("playerDropped", [players](const std::string& source, fx::EventArgs args)
    {
        const std::string reason = args.get<std::string>(0);
        const std::string name = players->count(source) ? (*players)[source] : source;
        players->erase(source);
        fx::trace("%s (id: %s) dropped (%s). Players online: %zu\n", name.c_str(), source.c_str(), reason.c_str(), players->size());
    });

    fx::on("chatMessage", [](const std::string& source, fx::EventArgs args)
    {
        fx::trace("chatMessage debug | source=%s args=%zu\n", source.c_str(), args.size());
        for (size_t i = 0; i < args.size(); ++i)
            fx::trace("  arg[%zu] = '%s'\n", i, args.str(i).c_str());
    });

    fx::onCommand("players", [players](const std::string&, const std::vector<std::string>&)
    {
        fx::trace("--- online players (%zu) ---\n", players->size());
        for (const auto& [id, name] : *players)
            fx::trace("  [%s] %s\n", id.c_str(), name.c_str());
        fx::trace("----------------------------\n");
    });
}
