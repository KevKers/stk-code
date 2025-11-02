//  Hide & Seek randomteams command
//  GPLv3-or-later

#include "randomteams_hs.hpp"

#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/server_config.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"
#include "race/race_manager.hpp"
#include "utils/string_utils.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <random>

static const char* LOGNAME = "RandomTeamsHS";

bool RandomTeamsHSCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);
    auto parser = ctx->get_parser();
    parser->parse_finish();

    ServerLobby* const lobby = stk_ctx->get_lobby();

    if (RaceManager::get()->getMinorMode() != RaceManager::MINOR_MODE_HIDE_SEEK)
    {
        ctx->write("This command is only for Hide and Seek mode.");
        ctx->flush();
        return false;
    }
    if (lobby->getCurrentState() != ServerLobby::WAITING_FOR_START_GAME)
    {
        ctx->write("Team assignment not possible during game.");
        ctx->flush();
        return false;
    }

    // No permission check - anyone can randomize teams
    CMD_VOTABLE(data, false);

    // Collect all non-spectator players
    std::vector<std::shared_ptr<NetworkPlayerProfile>> players;
    auto peers = STKHost::get()->getPeers();
    for (auto& peer : peers)
    {
        if (peer->alwaysSpectate()) continue;
        for (auto& p : peer->getPlayerProfiles())
        {
            players.push_back(p);
        }
    }

    if (players.size() < 2)
    {
        ctx->write("Not enough players.");
        ctx->flush();
        return false;
    }

    // Shuffle deterministically per call
    std::shuffle(players.begin(), players.end(), RandomGenerator::getGenerator());

    unsigned seekers = 1;
    if (players.size() >= 6 && players.size() <= 10) seekers = 2;
    // For >10 players, keep 2 seekers for now (can be adjusted later)

    unsigned assigned_seekers = 0;
    for (size_t i = 0; i < players.size(); i++)
    {
        if (assigned_seekers < seekers)
        {
            players[i]->setTeam(KART_TEAM_BLUE); // Seekers = Blue
            assigned_seekers++;
        }
        else
        {
            players[i]->setTeam(KART_TEAM_RED); // Hiders = Red
        }
    }

    lobby->updatePlayerList();

    // Announce plain labels (lobby UI will render colored by team, but we use text here)
    lobby->sendStringToAllPeers("Teams have been randomly assigned for Hide and Seek.");

    Log::info(LOGNAME, "Assigned %u seekers out of %u players.", seekers, (unsigned)players.size());

    return true;
}
