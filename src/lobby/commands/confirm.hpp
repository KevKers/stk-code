//  Hide & Seek /confirm command (player-only)
#ifndef LOBBY_COMMAND_CONFIRM_HPP
#define LOBBY_COMMAND_CONFIRM_HPP

#include "lobby/stk_command.hpp"
#include "network/moderation_toolkit/server_permission_level.hpp"

class ConfirmCommand : public STKCommand
{
public:
    ConfirmCommand() : STKCommand(false)
    {
        m_name = "confirm";
        m_description = "Hiders: confirm your hiding spot (immobilize kart).";
        // no args
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_CONFIRM_HPP