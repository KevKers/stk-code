#include "hint.hpp"

#include "lobby/stk_command_context.hpp"
#include "modes/world.hpp"
#include "modes/hide_seek_world.hpp"
#include "race/race_manager.hpp"
#include "utils/string_utils.hpp"
#include "network/server_config.hpp"

static inline const char* hs_bucket_for(float d)
{
    const float hot = (float)ServerConfig::m_hs_hot_threshold;
    const float wmin = (float)ServerConfig::m_hs_warm_min;
    const float wmax = (float)ServerConfig::m_hs_warm_max;
    if (d < hot) return "Hot";
    if (d >= wmin && d <= wmax) return "Warm";
    return "Cold";
}

bool HintCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
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

    // Let world compute the result and return message
    std::string out;
    bool ok = hs->handleHintFor(stk_ctx->getProfileName(), out);
    if (!ok)
    {
        ctx->write(out);
        ctx->flush();
        return false;
    }

    ctx->write(out);
    ctx->flush();
    return true;
}