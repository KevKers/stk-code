/* This file contains many functions. We only patch updateTracksForMode to add
 * track filtering for Hide & Seek mode. The rest of the file remains unchanged. */
#include "network/protocols/server_lobby.hpp"
#include "io/file_manager.hpp"
#include "lobby/rps_challenge.hpp"
#include "network/event.hpp"
#include "network/network_string.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"
#include "network/network_player_profile.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "utils/time.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
// other includes are already present at top of this file in repository

// ... existing code above ...

void ServerLobby::updateTracksForMode()
{
    auto all_t = track_manager->getAllTrackIdentifiers();
    if (all_t.size() >= 65536)
        all_t.resize(65535);
    m_available_kts.second = { all_t.begin(), all_t.end() };
    RaceManager::MinorRaceModeType m =
        ServerConfig::getLocalGameMode(m_game_mode.load()).first;
    switch (m)
    {
        case RaceManager::MINOR_MODE_NORMAL_RACE:
        case RaceManager::MINOR_MODE_TIME_TRIAL:
        case RaceManager::MINOR_MODE_FOLLOW_LEADER:
        case RaceManager::MINOR_MODE_HIDE_SEEK: // HS: use race tracks (exclude arenas/ctf/soccer)
        {
            auto it = m_available_kts.second.begin();
            while (it != m_available_kts.second.end())
            {
                Track* t =  track_manager->getTrack(*it);
                if (t->isArena() || t->isSoccer() || t->isInternal())
                {
                    it = m_available_kts.second.erase(it);
                }
                else
                    it++;
            }
            break;
        }
        case RaceManager::MINOR_MODE_FREE_FOR_ALL:
        case RaceManager::MINOR_MODE_CAPTURE_THE_FLAG:
        {
            auto it = m_available_kts.second.begin();
            while (it != m_available_kts.second.end())
            {
                Track* t =  track_manager->getTrack(*it);
                if (RaceManager::get()->getMinorMode() ==
                    RaceManager::MINOR_MODE_CAPTURE_THE_FLAG)
                {
                    if (!t->isCTF() || t->isInternal())
                    {
                        it = m_available_kts.second.erase(it);
                    }
                    else
                        it++;
                }
                else
                {
                    if (!t->isArena() ||  t->isInternal())
                    {
                        it = m_available_kts.second.erase(it);
                    }
                    else
                        it++;
                }
            }
            break;
        }
        case RaceManager::MINOR_MODE_SOCCER:
        {
            auto it = m_available_kts.second.begin();
            while (it != m_available_kts.second.end())
            {
                Track* t =  track_manager->getTrack(*it);
                if (!t->isSoccer() || t->isInternal())
                {
                    it = m_available_kts.second.erase(it);
                }
                else
                    it++;
            }
            break;
        }
        default:
            assert(false);
            break;
    }

}   // updateTracksForMode

void ServerLobby::loadJumbleWordList()
{
    std::string words_file = file_manager->getAsset("jumble_words.txt");
    std::ifstream file(words_file);
    if (!file.is_open())
    {
        Log::warn("ServerLobby", "Could not load jumble_words.txt, using default words");
        m_jumble_word_list = {"kart", "race", "track", "speed", "drift", "nitro", "boost"};
        return;
    }
    
    std::string word;
    while (std::getline(file, word))
    {
        word.erase(word.find_last_not_of(" \n\r\t") + 1);
        if (!word.empty() && word.length() > 2)
        {
            m_jumble_word_list.push_back(word);
        }
    }
    file.close();
    
    if (m_jumble_word_list.empty())
    {
        m_jumble_word_list = {"kart", "race", "track", "speed", "drift", "nitro", "boost"};
    }
    
    Log::info("ServerLobby", "Loaded %d words for jumble game", (int)m_jumble_word_list.size());
}

std::string ServerLobby::jumbleWord(const std::string& word)
{
    std::string jumbled = word;
    std::shuffle(jumbled.begin(), jumbled.end(), m_jumble_rng);
    
    int attempts = 0;
    while (jumbled == word && attempts < 10)
    {
        std::shuffle(jumbled.begin(), jumbled.end(), m_jumble_rng);
        attempts++;
    }
    
    return jumbled;
}

void ServerLobby::startJumbleForPlayer(uint32_t player_id)
{
    if (m_jumble_word_list.empty())
    {
        loadJumbleWordList();
    }
    
    std::uniform_int_distribution<size_t> dist(0, m_jumble_word_list.size() - 1);
    std::string word = m_jumble_word_list[dist(m_jumble_rng)];
    std::string jumbled = jumbleWord(word);
    
    m_jumble_player_words[player_id] = StringUtils::toLowerCase(word);
    m_jumble_player_jumbled[player_id] = jumbled;
    m_jumble_player_start_time[player_id] = StkTime::getMonoTimeMs();
    
    std::shared_ptr<STKPeer> peer = nullptr;
    for (auto& p : STKHost::get()->getPeers())
    {
        if (p->getHostId() == player_id)
        {
            peer = p;
            break;
        }
    }
    
    if (peer)
    {
        std::string msg = "Unscramble this word: " + jumbled;
        sendStringToPeer(msg, peer);
    }
}

void ServerLobby::endJumbleForPlayer(uint32_t player_id, bool won)
{
    auto it_word = m_jumble_player_words.find(player_id);
    auto it_start = m_jumble_player_start_time.find(player_id);
    
    if (it_word == m_jumble_player_words.end())
        return;
    
    std::shared_ptr<STKPeer> peer = nullptr;
    std::string player_name;
    
    for (auto& p : STKHost::get()->getPeers())
    {
        if (p->getHostId() == player_id)
        {
            peer = p;
            if (p->hasPlayerProfiles())
            {
                player_name = StringUtils::wideToUtf8(p->getPlayerProfiles()[0]->getName());
            }
            break;
        }
    }
    
    if (won && peer)
    {
        uint64_t time_taken = StkTime::getMonoTimeMs() - it_start->second;
        double seconds = time_taken / 1000.0;
        
        std::stringstream ss;
        ss << player_name << " correctly unscrambled '" << it_word->second 
           << "' in " << std::fixed << std::setprecision(2) << seconds << " seconds!";
        sendStringToAllPeers(ss.str());
    }
    
    m_jumble_player_words.erase(player_id);
    m_jumble_player_jumbled.erase(player_id);
    m_jumble_player_start_time.erase(player_id);
}

void ServerLobby::updateJumbleTimer()
{
    uint64_t now = StkTime::getMonoTimeMs();
    
    for (auto it = m_jumble_player_start_time.begin(); it != m_jumble_player_start_time.end();)
    {
        if (now - it->second > 60000)
        {
            uint32_t player_id = it->first;
            
            std::shared_ptr<STKPeer> peer = nullptr;
            for (auto& p : STKHost::get()->getPeers())
            {
                if (p->getHostId() == player_id)
                {
                    peer = p;
                    break;
                }
            }
            
            if (peer)
            {
                sendStringToPeer("Time's up! The game has ended.", peer);
            }
            
            m_jumble_player_words.erase(player_id);
            m_jumble_player_jumbled.erase(player_id);
            it = m_jumble_player_start_time.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ServerLobby::checkRPSTimeouts()
{
    uint64_t now = StkTime::getMonoTimeMs();
    
    for (auto it = m_rps_challenges.begin(); it != m_rps_challenges.end();)
    {
        if (now > it->timeout)
        {
            std::shared_ptr<STKPeer> challenger_peer = nullptr;
            std::shared_ptr<STKPeer> challenged_peer = nullptr;
            
            for (auto& p : STKHost::get()->getPeers())
            {
                if (p->getHostId() == it->challenger_id)
                    challenger_peer = p;
                if (p->getHostId() == it->challenged_id)
                    challenged_peer = p;
            }
            
            if (!it->accepted)
            {
                if (challenger_peer)
                    sendStringToPeer("Rock Paper Scissors challenge timed out.", challenger_peer);
                if (challenged_peer)
                    sendStringToPeer("Rock Paper Scissors challenge timed out.", challenged_peer);
            }
            else
            {
                if (challenger_peer)
                    sendStringToPeer("Rock Paper Scissors game timed out - no winner.", challenger_peer);
                if (challenged_peer)
                    sendStringToPeer("Rock Paper Scissors game timed out - no winner.", challenged_peer);
            }
            
            it = m_rps_challenges.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ServerLobby::determineRPSWinner(RPSChallenge& challenge)
{
    std::shared_ptr<STKPeer> challenger_peer = nullptr;
    std::shared_ptr<STKPeer> challenged_peer = nullptr;
    
    for (auto& p : STKHost::get()->getPeers())
    {
        if (p->getHostId() == challenge.challenger_id)
            challenger_peer = p;
        if (p->getHostId() == challenge.challenged_id)
            challenged_peer = p;
    }
    
    std::string result;
    std::string challenger_choice = RockPaperScissors::rpsToString(challenge.challenger_choice);
    std::string challenged_choice = RockPaperScissors::rpsToString(challenge.challenged_choice);
    
    if (challenge.challenger_choice == challenge.challenged_choice)
    {
        result = "It's a tie! Both chose " + challenger_choice + ".";
    }
    else if ((challenge.challenger_choice == RPS_ROCK && challenge.challenged_choice == RPS_SCISSORS) ||
             (challenge.challenger_choice == RPS_PAPER && challenge.challenged_choice == RPS_ROCK) ||
             (challenge.challenger_choice == RPS_SCISSORS && challenge.challenged_choice == RPS_PAPER))
    {
        result = challenge.challenger_name + " wins with " + challenger_choice + " vs " + challenged_choice + "!";
    }
    else
    {
        result = challenge.challenged_name + " wins with " + challenged_choice + " vs " + challenger_choice + "!";
    }
    
    sendStringToAllPeers(result);
    
    for (auto it = m_rps_challenges.begin(); it != m_rps_challenges.end(); ++it)
    {
        if (it->challenger_id == challenge.challenger_id && it->challenged_id == challenge.challenged_id)
        {
            m_rps_challenges.erase(it);
            break;
        }
    }
}

void ServerLobby::handleChat(Event* event)
{
    if (!event || event->getType() != EVENT_TYPE_MESSAGE)
        return;
        
    std::shared_ptr<STKPeer> peer = event->getPeerSP();
    if (!peer || !peer->isValidated())
        return;
    
    peer->updateLastActivity();
    
    NetworkString& data = event->data();
    core::stringw message;
    data.decodeStringW(&message);
    std::string message_utf8 = StringUtils::wideToUtf8(message);
    
    std::string message_check = message_utf8;
    size_t colon_pos = message_check.find(": ");
    if (colon_pos != std::string::npos)
    {
        message_check = message_check.substr(colon_pos + 2);
    }
    
    std::string message_lower = StringUtils::toLowerCase(message_check);
    message_lower.erase(0, message_lower.find_first_not_of(" \t\n\r"));
    message_lower.erase(message_lower.find_last_not_of(" \t\n\r") + 1);
    
    uint32_t peer_id = peer->getHostId();
    
    {
        std::lock_guard<std::mutex> lock(m_jumble_mutex);
        auto it_word = m_jumble_player_words.find(peer_id);
        if (it_word != m_jumble_player_words.end())
        {
            if (message_lower == it_word->second)
            {
                endJumbleForPlayer(peer_id, true);
                return;
            }
        }
    }
    
    NetworkString* chat = getNetworkString();
    chat->setSynchronous(true);
    chat->addUInt8(LE_CHAT).encodeString16(message);
    
    STKHost::get()->sendPacketToAllPeers(chat);
    delete chat;
}

// ... rest of file unchanged ...