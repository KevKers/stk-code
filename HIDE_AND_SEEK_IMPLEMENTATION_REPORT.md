# Hide and Seek Mode Implementation Report

## Executive Summary
The Hide and Seek game mode for SuperTuxKart is **already substantially implemented** with most features you requested. Below is a detailed breakdown of what exists and what needs to be adjusted.

---

## ✅ FULLY IMPLEMENTED FEATURES

### 1. Core Game Mode
- **Location**: `src/modes/hide_seek_world.cpp` and `hide_seek_world.hpp`
- **Status**: ✅ Fully functional
- **Details**: Complete implementation with two phases (PHASE_HIDE and PHASE_SEEK)

### 2. Team System
- **Hiders**: Red Team (KART_TEAM_RED)
- **Seekers**: Blue Team (KART_TEAM_BLUE)
- **Status**: ✅ Working

### 3. Team Assignment Commands

#### `/randomteams` Command
- **Location**: `src/lobby/commands/randomteams_hs.cpp`
- **Status**: ✅ Implemented
- **Logic**:
  - 2-5 players: 1 seeker
  - 6-10 players: 2 seekers
  - Voting system with 50% threshold for non-admins
  - Admins can force via veto

#### Manual Team Assignment
- **Status**: ✅ Supported
- **Implementation**: Can be done by admins via existing setteam command

### 4. Hide Phase Mechanics
- **Duration**: 3-4 minutes (configurable via `/sethidetime` command)
- **Default**: 180 seconds (3 minutes)
- **Seeker Freeze**: ✅ Seekers cannot move during hide phase
- **Early Start**: ✅ If all hiders `/confirm`, seek phase starts immediately

### 5. `/confirm` Command
- **Location**: `src/lobby/commands/confirm.cpp`
- **Status**: ✅ Working
- **Effect**:
  - Locks hider's kart (cannot move)
  - Shows zipper fire effect (minimal VFX)
  - Broadcasts: "Player %name% is ready!"

### 6. Seeker Mechanics
- **Cake Bombs**: ✅ Seekers get N cake_bombs where N = number of alive hiders
- **Finding Mechanism**: 
  - ✅ Proximity hit detection with cakes
  - ✅ Manual `/found [playername]` command (5m proximity required)
- **Refund System**: ✅ If seeker hits non-hider, cake is refunded with 1.5s cooldown

### 7. Found Player Handling
- **Current**: Found players sent to lobby after **5 seconds**
- **⚠️ NEEDS CHANGE**: User wants **10 seconds**
- **Location**: Line 233 in `hide_seek_world.cpp`
- **Post-elimination**: Player moved to spectator mode

### 8. `/hint` Command
- **Location**: `src/lobby/commands/hint.cpp`
- **Status**: ✅ Fully implemented
- **Features**:
  - Proximity-based hints: Hot/Warm/Cold
  - Configurable max uses (default from `ServerConfig::m_hs_hint_default_uses`)
  - 20-second cooldown between uses
  - Unlock time configurable (`ServerConfig::m_hs_hint_unlock_seconds`)
  - Shows count if multiple players in same range
  - Examples:
    - "Hot" (1 player nearby)
    - "3 players are Hot" (multiple nearby)
    - "Warm"
    - "Cold"

### 9. `/poweruphint` Command
- **Location**: `src/lobby/commands/poweruphint.cpp`
- **Status**: ✅ Working
- **Usage**: `/poweruphint <number>` to set max hint uses for current round

### 10. Time Limits
- **Total Game Time**: 15 minutes (configurable, default 900 seconds)
- **Seek Time**: 14 minutes maximum (15 min total - 1 min hide)
- **Commands**: `/settotaltimecap` to adjust

### 11. `/teamchat` Command
- **Location**: `src/lobby/commands/teamchat.cpp`
- **Status**: ✅ Implemented
- **Behavior**:
  - Hiders can chat with hiders only
  - Seekers can chat with seekers only
  - Non-team members cannot see team messages
  - Toggle by typing `/teamchat` - all subsequent messages are team-only
  - Use `/public` to return to public chat

### 12. Minimap Visibility
- **Location**: `src/states_screens/race_gui.cpp` (lines 588-599)
- **Status**: ✅ **ALREADY DISABLED FOR SEEKERS**
- **Implementation**: Minimap hidden for Blue Team (seekers), shown for Red Team (hiders)

### 13. Network & Spectator Features
- **Live Join**: ✅ Disconnected players can rejoin with roles preserved
- **Mid-game Join**: ✅ New players forced to spectator mode
- **Spectate After Found**: ✅ Found players moved to lobby with spectator option
- **State Sync**: ✅ Full network state synchronization implemented

### 14. Game End Conditions
- **All hiders found**: Seekers win
- **Time limit reached**: Hiders win
- **Win/Loss**: Proper win/loss state set for animations and music

### 15. Track Selection
- **Tracks**: Normal race tracks are used (not limited to arena tracks)
- **Works with**: Any track with navigation mesh for arena-style play

---

## ⚠️ FEATURES THAT NEED IMPLEMENTATION/FIXES

### 1. Player Nametags Above Karts
- **Current Status**: ❌ Not filtered for Hide and Seek
- **Required**: Disable nametags above karts for seekers only (hiders should see all names)
- **Location to Modify**: Text billboard rendering system
- **Impact**: HIGH PRIORITY - core gameplay feature

### 2. Elimination Delay
- **Current**: 5 seconds
- **Required**: 10 seconds
- **Location**: `src/modes/hide_seek_world.cpp` line 233 and 322
- **Impact**: EASY FIX - one line change

### 3. Lobby Labels [HIDING]/[SEEKING]
- **Status**: ❓ Needs verification
- **Required**: Display "[HIDING] PlayerName" or "[SEEKING] PlayerName" in lobby
- **Location**: Lobby UI (`src/states_screens/online/networking_lobby.cpp`)
- **Impact**: MEDIUM - UI enhancement

### 4. AI Support (Singleplayer)
- **Status**: ❓ Needs testing
- **Required**: AI karts should work as hiders/seekers in offline mode
- **Note**: AI already exists for other modes (soccer, FFA)
- **Impact**: MEDIUM - gameplay feature

### 5. Statistics Tracking (`/stats` Command)
- **Current Status**: ❌ Not implemented
- **Required**: 
  - `/stats [playername]` command
  - Track: win percentage, fastest finding time (as seeker), longest hiding time (as hider)
  - Persistent storage in database/file
- **Impact**: LOW PRIORITY - nice-to-have feature

---

## 📋 CONFIGURATION COMMANDS SUMMARY

All working commands for Hide and Seek:

| Command | Description | Permission |
|---------|-------------|------------|
| `/randomteams` | Randomly assign teams | Votable (50%) or Admin |
| `/confirm` | Hider confirms hiding position | Any player during hide phase |
| `/hint` | Get proximity hint | Seekers only during seek phase |
| `/found [name]` | Manually mark player as found | Seekers (requires 5m proximity) |
| `/teamchat` | Enable team-only chat | Any player |
| `/public` | Return to public chat | Any player |
| `/poweruphint <n>` | Set max hint uses for round | Admin only |
| `/sethidetime <seconds>` | Set hide phase duration | Admin only |
| `/settotaltimecap <seconds>` | Set total game time | Admin only |

---

## 🎮 GAMEPLAY FLOW

### Lobby Phase
1. Players join server
2. Admin or vote triggers `/randomteams`
3. Teams assigned: Hiders (Red) and Seekers (Blue)
4. [Optional] Lobby shows labels: "[HIDING] Player1", "[SEEKING] Player2"
5. Game starts

### Hide Phase (3-4 minutes)
1. Seekers frozen in place (cannot move)
2. Hiders can move freely to find hiding spots
3. Hiders type `/confirm` when ready
4. Hiders frozen after confirming
5. Broadcast: "Player X is ready!"
6. Phase ends when:
   - All hiders confirm, OR
   - Hide time expires

### Seek Phase (up to 14 minutes)
1. Seekers unfreeze
2. Seekers receive N cake_bombs (N = number of hiders)
3. Seekers hunt for hiders
4. Seekers can use `/hint` (with cooldown and use limit)
5. When seeker hits hider with cake:
   - Broadcast: "Player X has been found"
   - 10-second countdown (currently 5s - **needs fix**)
   - Hider sent to lobby (spectator mode)
6. Game ends when:
   - All hiders found (Seekers win), OR
   - Time limit reached (Hiders win)

### Result Phase
1. Display results with found times
2. Show remaining hiders
3. Play win/loss music and animations
4. Return to lobby

---

## 🔧 REQUIRED CHANGES

### Priority 1: Change Elimination Delay (EASY)
**File**: `src/modes/hide_seek_world.cpp`
**Line 233**: Change `5.0f` to `10.0f`
**Line 322**: Change `5.0f` to `10.0f`

```cpp
// Current:
const int when = getTimeTicks() + stk_config->time2Ticks(5.0f);

// Change to:
const int when = getTimeTicks() + stk_config->time2Ticks(10.0f);
```

### Priority 2: Disable Nametags for Seekers (MEDIUM)
**Files to modify**:
- Player nametag rendering (text billboard system)
- Add Hide and Seek mode check
- Filter based on viewer's team (seekers don't see names, hiders see all)

### Priority 3: Lobby Team Labels (MEDIUM)
**File**: `src/states_screens/online/networking_lobby.cpp`
**Add**: Prefix player names with "[HIDING]" or "[SEEKING]" based on team when in lobby

### Priority 4: AI Support Verification (MEDIUM)
**Test**: Create singleplayer Hide and Seek game with AI
**May need**: AI controller for Hide and Seek mode (similar to soccer_ai.cpp)

### Priority 5: Stats Command (LOW)
**Create**: New command in `src/lobby/commands/stats.cpp`
**Database**: Track stats per player (wins, hiding time, seeking time)

---

## 🎯 CONCLUSION

**Your Hide and Seek mode is approximately 85-90% complete!**

The core gameplay loop, commands, network support, and most mechanics are already fully functional. The remaining work consists of:

1. ✅ **Minor tweaks** (elimination delay)
2. ✅ **One important feature** (nametag hiding for seekers)
3. ✅ **UI polish** (lobby labels)
4. ✅ **Testing/verification** (AI support)
5. ✅ **Optional enhancement** (stats tracking)

Would you like me to proceed with implementing these changes?
