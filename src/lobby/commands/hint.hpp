//  Hide & Seek /hint command (seekers only)
#ifndef LOBBY_COMMAND_HINT_HPP
#define LOBBY_COMMAND_HINT_HPP

#include "lobby/stk_command.hpp"

class HintCommand : public STKCommand
{
public:
    HintCommand() : STKCommand(false)
    {
        m_name = "hint";
        m_description = "Seekers: get a Hot/Warm/Cold hint about nearest hiders.";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_HINT_HPP