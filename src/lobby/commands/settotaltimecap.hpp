//  Hide & Seek settotaltimecap command (admin-only)
#ifndef LOBBY_COMMAND_SETTOTALTIMECAP_HPP
#define LOBBY_COMMAND_SETTOTALTIMECAP_HPP

#include "lobby/stk_command.hpp"
#include "network/moderation_toolkit/server_permission_level.hpp"

class SetTotalTimeCapCommand : public STKCommand
{
    ServerPermissionLevel m_required_perm = PERM_ADMINISTRATOR;
public:
    SetTotalTimeCapCommand() : STKCommand(false)
    {
        m_name = "settotaltimecap";
        m_args = {{nnwcli::CT_INTEGER, "seconds", "Total game time cap in seconds"}};
        m_description = "Set Hide & Seek total time cap (admin)";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_SETTOTALTIMECAP_HPP
