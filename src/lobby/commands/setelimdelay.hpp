//  Hide & Seek setelimdelay command (admin-only)
#ifndef LOBBY_COMMAND_SETELIMDELAY_HPP
#define LOBBY_COMMAND_SETELIMDELAY_HPP

#include "lobby/stk_command.hpp"
#include "network/moderation_toolkit/server_permission_level.hpp"

class SetElimDelayCommand : public STKCommand
{
    ServerPermissionLevel m_required_perm = PERM_ADMINISTRATOR;
public:
    SetElimDelayCommand() : STKCommand(false)
    {
        m_name = "setelimdelay";
        m_args = {{nnwcli::CT_FLOAT, "seconds", "Elimination delay in seconds"}};
        m_description = "Set Hide & Seek elimination delay (admin)";
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_SETELIMDELAY_HPP
