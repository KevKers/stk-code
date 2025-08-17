#include "lobby/commands/poweruphint.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/server_config.hpp"
#include "modes/hide_seek_world.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "utils/string_utils.hpp"

bool PowerUpHintCommand::execute(nnwcli::CommandExecutorContext* ctx, void* data)
{
    // Permission check
    auto sl = LobbyProtocol::get<ServerLobby>();
    if (!sl) return false;
    if (!sl->hasPermission(ctx->getPeer(), m_required_perm))
    {
        ctx->addLog(StringUtils::wideToUtf8(ServerConfig::m_permission_message.get()));
        ctx->flush();
        return false;
    }

    int uses = 0;
    ctx->getInt("uses", &uses);
    if (uses < 0) uses = 0;

    // Check unlock time: only after hs-hint-unlock-seconds
    World* w = World::getWorld();
    HideAndSeekWorld* hs = dynamic_cast<HideAndSeekWorld*>(w);
    if (!hs)
    {
        ctx->addLog("Hide & Seek round not active.");
        ctx->flush();
        return false;
    }

    // Only allow after unlock seconds
    int unlock = (int)ServerConfig::m_hs_hint_unlock_seconds;
    if (w->getTicksSinceStart() < stk_config->time2Ticks((float)unlock))
    {
        ctx->addLog("You can only change hint uses after the hint unlock time.");
        ctx->flush();
        return false;
    }

    // Set for this round
    hs->setHintMaxUsesForRound(uses);
    std::string msg = StringUtils::insertValues("Set /hint uses to %d for this round.", uses);
    ctx->addLog(msg);
    ctx->flush();
    return true;
}
