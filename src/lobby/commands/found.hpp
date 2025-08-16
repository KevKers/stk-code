//  Hide & Seek /found <player> command (seeker manual find with proximity)
#ifndef LOBBY_COMMAND_FOUND_HPP
#define LOBBY_COMMAND_FOUND_HPP

#include "lobby/stk_command.hpp"

class FoundCommand : public STKCommand
{
public:
    FoundCommand() : STKCommand(false)
    {
        m_name = "found";
        m_args = {{nnwcli::CT_STRING, "player", "Target player name"}};
        m_description = "Seekers: manually mark a nearby hider as found (<=5m).";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_FOUND_HPP