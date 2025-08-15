//  Hide & Seek sethidetime command (admin-only)
#ifndef LOBBY_COMMAND_SETHIDETIME_HPP
#define LOBBY_COMMAND_SETHIDETIME_HPP

#include "lobby/stk_command.hpp"
#include "network/moderation_toolkit/server_permission_level.hpp"

class SetHideTimeCommand : public STKCommand
{
    ServerPermissionLevel m_required_perm = PERM_ADMINISTRATOR;
public:
    SetHideTimeCommand() : STKCommand(false)
    {
        m_name = "sethidetime";
        m_args = {{nnwcli::CT_INTEGER, "seconds", "Hide phase length in seconds"}};
        m_description = "Set Hide & Seek hide phase duration (admin)";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_SETHIDETIME_HPP
