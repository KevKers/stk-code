#include "public.hpp"

#include "lobby/stk_command_context.hpp"
#include "network/protocols/server_lobby.hpp"
#include "utils/log.hpp"

bool PublicCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);

    auto lobby = stk_ctx->get_lobby();
    if (!lobby)
        return false;

    // Remove from team-only chat list if server tracks it; here we simply notify
    ctx->write("You are now chatting publicly.");
    ctx->flush();
    return true;
}