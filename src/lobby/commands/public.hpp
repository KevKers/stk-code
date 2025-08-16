//  /public command to exit teamchat
#ifndef LOBBY_COMMAND_PUBLIC_HPP
#define LOBBY_COMMAND_PUBLIC_HPP

#include "lobby/stk_command.hpp"

class PublicCommand : public STKCommand
{
public:
    PublicCommand() : STKCommand(false)
    {
        m_name = "public";
        m_description = "Exit team-only chat and send messages to everyone.";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_PUBLIC_HPP