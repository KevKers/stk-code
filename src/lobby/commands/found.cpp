#include "found.hpp"

#include "lobby/stk_command_context.hpp"
#include "modes/world.hpp"
#include "modes/hide_seek_world.hpp"
#include "race/race_manager.hpp"
#include "utils/string_utils.hpp"

#include <parser/argline_parser.hpp>

bool FoundCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);
    auto parser = ctx->get_parser();

    if (RaceManager::get()->getMinorMode() != RaceManager::MINOR_MODE_HIDE_SEEK)
    {
        ctx->write("This command is only for Hide and Seek mode.");
        ctx->flush();
        return false;
    }

    std::string target;
    *parser >> target;
    parser->parse_finish();

    World* w = World::getWorld();
    HideAndSeekWorld* hs = dynamic_cast<HideAndSeekWorld*>(w);
    if (!hs)
    {
        ctx->write("World is not Hide and Seek.");
        ctx->flush();
        return false;
    }

    const std::string seeker = stk_ctx->getProfileName();

    // Require proximity <= 5m
    const bool ok = hs->manualFoundByName(seeker, target, 5.0f);
    if (!ok)
    {
        ctx->write("There are no players nearby to broadcast the message");
        ctx->flush();
        return false;
    }

    return true;
}