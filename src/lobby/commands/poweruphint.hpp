//  Hide & Seek /poweruphint <n> (admin-only) to set per-seeker hint uses for this round
#ifndef LOBBY_COMMAND_POWERUPHINT_HPP
#define LOBBY_COMMAND_POWERUPHINT_HPP

#include "lobby/stk_command.hpp"
#include "network/moderation_toolkit/server_permission_level.hpp"

class PowerUpHintCommand : public STKCommand
{
    ServerPermissionLevel m_required_perm = PERM_ADMINISTRATOR;
public:
    PowerUpHintCommand() : STKCommand(false)
    {
        m_name = "poweruphint";
        m_args = {{nnwcli::CT_INTEGER, "uses", "Per-seeker /hint uses for this round"}};
        m_description = "Set Hide & Seek /hint uses for this round (admin).";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_POWERUPHINT_HPP
