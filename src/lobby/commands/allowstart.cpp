#include "lobby/commands/allowstart.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "network/protocols/server_lobby.hpp"
#include "utils/string_utils.hpp"

bool AllowStartCommand::execute(nnwcli::CommandExecutorContext* ctx, void* data)
{
    auto sl = LobbyProtocol::get<ServerLobby>();
    if (!sl) return false;
    if (!sl->hasPermission(ctx->getPeer(), m_required_perm))
    {
        ctx->addLog(StringUtils::wideToUtf8(ServerConfig::m_permission_message.get()));
        ctx->flush();
        return false;
    }

    int allow = 1;
    ctx->getInt("allow", &allow);
    allow = allow ? 1 : 0;
    sl->setAllowStart(allow != 0);

    ctx->addLog(std::string("allowstart set to ") + (allow ? "1" : "0"));
    ctx->flush();
    return true;
}
