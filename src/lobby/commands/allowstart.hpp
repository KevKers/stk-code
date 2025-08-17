//  Admin-only /allowstart [0|1] toggle to disable/enable readying for all peers
#ifndef LOBBY_COMMAND_ALLOWSTART_HPP
#define LOBBY_COMMAND_ALLOWSTART_HPP

#include "lobby/stk_command.hpp"
#include "network/moderation_toolkit/server_permission_level.hpp"

class AllowStartCommand : public STKCommand
{
    ServerPermissionLevel m_required_perm = PERM_ADMINISTRATOR;
public:
    AllowStartCommand() : STKCommand(false)
    {
        m_name = "allowstart";
        m_args = {{nnwcli::CT_INTEGER, "allow", "0 to disallow starting; 1 to allow"}};
        m_description = "Admin: toggle if players can Ready/Start.";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_ALLOWSTART_HPP
