#include "confirm.hpp"

#include "lobby/stk_command_context.hpp"
#include "modes/world.hpp"
#include "modes/hide_seek_world.hpp"
#include "race/race_manager.hpp"
#include "utils/string_utils.hpp"

bool ConfirmCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);

    if (RaceManager::get()->getMinorMode() != RaceManager::MINOR_MODE_HIDE_SEEK)
    {
        ctx->write("This command is only for Hide and Seek mode.");
        ctx->flush();
        return false;
    }

    World* w = World::getWorld();
    HideAndSeekWorld* hs = dynamic_cast<HideAndSeekWorld*>(w);
    if (!hs)
    {
        ctx->write("World is not Hide and Seek.");
        ctx->flush();
        return false;
    }

    // Find the caller's kart id by profile name
    const std::string myname = stk_ctx->getProfileName();
    int my_kart = -1;
    for (unsigned i = 0; i < w->getNumKarts(); ++i)
    {
        const std::string nm = StringUtils::wideToUtf8(
            w->getKart(i)->getController()->getName());
        if (nm == myname) { my_kart = (int)i; break; }
    }

    if (my_kart < 0)
    {
        ctx->write("Unable to identify your kart.");
        ctx->flush();
        return false;
    }

    // Check if player is a hider (Red team)
    if (w->getKartTeam(my_kart) != KART_TEAM_RED)
    {
        ctx->write("Only hiders can use this command.");
        ctx->flush();
        return false;
    }

    if (!hs->confirmHiderKart(my_kart))
    {
        ctx->write("You cannot confirm right now.");
        ctx->flush();
        return false;
    }

    // Success: no additional output, the world broadcasted the message already
    return true;
}