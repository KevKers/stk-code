//  Hide & Seek randomteams command
//  GPLv3-or-later
#ifndef LOBBY_COMMAND_RANDOMTEAMS_HS_HPP
#define LOBBY_COMMAND_RANDOMTEAMS_HS_HPP

#include "lobby/stk_command.hpp"
#include "network/moderation_toolkit/server_permission_level.hpp"

class RandomTeamsHSCommand : public STKCommand
{
    // Let everyone vote; admin can force
    ServerPermissionLevel m_min_veto = PERM_ADMINISTRATOR;
public:
    RandomTeamsHSCommand() : STKCommand(true/*votable*/)
    {
        m_name = "randomteams";
        m_description = "Assigns Hiders/Seekers for Hide and Seek (1 seeker for <=5 players, 2 for 6-10).";
        // no args
    }
    virtual bool execute(nnwcli::CommandExecutorContext* context, void* data) OVERRIDE;
};

#endif // LOBBY_COMMAND_RANDOMTEAMS_HS_HPP
