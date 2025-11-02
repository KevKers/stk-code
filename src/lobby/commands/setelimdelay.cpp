#include "setelimdelay.hpp"

#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/server_config.hpp"
#include "race/race_manager.hpp"
#include "utils/log.hpp"

#include <parser/argline_parser.hpp>

bool SetElimDelayCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);
    auto parser = ctx->get_parser();

    float seconds = -1.0f;
    *parser >> seconds;
    parser->parse_finish();

    CMD_REQUIRE_PERM(stk_ctx, m_required_perm);

    if (RaceManager::get()->getMinorMode() != RaceManager::MINOR_MODE_HIDE_SEEK)
    {
        ctx->write("This command is only for Hide and Seek mode.");
        ctx->flush();
        return false;
    }
    if (seconds <= 0.0f)
    {
        ctx->write("Specify a positive number of seconds.");
        ctx->flush();
        return false;
    }

    ServerConfig::m_hs_elimination_delay = seconds;
    ctx->nprintf("Elimination delay set to %.1f seconds.", 256, seconds);
    ctx->flush();
    return true;
}
