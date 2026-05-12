RegisterCommand("count", function(source)
        print(("[example:lua] Player count from C++: %d"):format(exports.example:getPlayerCount()))
end, false)

RegisterCommand("whoami", function(source)
        local name = exports.example:getPlayerName(tostring(source))
        if name then
                print(("[example:lua] Player %d is: %s"):format(source, name))
        else
                print(("[example:lua] Player %d not found"):format(source))
        end
end, false)
