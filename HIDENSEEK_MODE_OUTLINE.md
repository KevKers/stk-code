# Hide and Seek Game Mode - Implementation Outline

## Overview
Hide and Seek is a team-based multiplayer game mode for SuperTuxKart where one team (Hiders) tries to survive while the other team (Seekers) tries to find and eliminate them within a time limit.

## Game Flow

### Phase 1: Hide Phase (default 180 seconds, configurable)
- **Hiders (Red Team)**: Can move freely to hide themselves on the track
- **Seekers (Blue Team)**: Completely frozen in place, cannot see minimap
- **Transition Condition**: Either all hiders use `/confirm` command OR the hide time expires

### Phase 2: Seek Phase (until time cap, default 900 seconds total)
- **Hiders (Red Team)**: Try to evade seekers and survive
- **Seekers (Blue Team)**: Hunt down hiders using projectiles or physical contact
- **End Condition**: Either all hiders are eliminated OR the total time cap is reached

## Win Conditions
- **Seekers Win**: If ALL hiders are found and eliminated before the total time cap expires
- **Hiders Win**: If ANY hider survives until the total time cap expires

## Teams
- **Red Team (KART_TEAM_RED)**: Hiders
- **Blue Team (KART_TEAM_BLUE)**: Seekers
- Teams are assigned before the race starts using `/randomteams` command

## Key Mechanics

### Finding Hiders
Seekers can find hiders in two ways:
1. **Physical Contact**: Seeker kart bumps/touches a hider kart (detected in collision handling)
2. **Smart Cakes**: Seeker hits hider with a cake projectile

When found, the hider:
- Is announced globally: "Player [name] has been found"
- Has their found time recorded
- Is moved back to lobby after a configurable delay (default 5 seconds)
- Can spectate the remaining game or leave the server from lobby

### For Seekers:
1. **Smart Cakes**: Seekers receive "Smart Cakes" (homing projectiles) equal to the number of alive hiders
   - Smart cakes have enhanced targeting in 7-10m range to prefer nearest hider
   - Implemented via `SmartCake` class derived from `Cake`

2. **Distance Gating**: Seekers can only fire when within a certain distance of a hider
   - Configurable via `m_hs_fire_distance_threshold` in ServerConfig
   - If seeker tries to fire when too far, projectile is refunded with 1.5s cooldown

3. **Refund on Miss**: If a seeker hits a non-hider (another seeker), the cake is refunded
   - 1.5s cooldown applied after refund

4. **Item Boxes**: Seekers CANNOT pick up items from bonus boxes
   - Implemented in `ItemManager::checkItemHit()`

5. **Minimap**: Seekers CANNOT see the minimap during both phases
   - Prevents minimap-based tracking of hiders

6. **Hint System** (`/hint` command):
   - **Seeker-only command** (hiders cannot use it)
   - Limited uses per round (configurable via `m_hs_hint_default_uses`)
   - 20-second cooldown between hints
   - Unlock time configurable (default 0 seconds from game start)
   - Returns distance category to nearest hider:
     - **Hot**: < `m_hs_hot_threshold` meters
     - **Warm**: Between `m_hs_warm_min` and `m_hs_warm_max` meters
     - **Cold**: Beyond warm range
   - If multiple hiders in a category, shows count

### For Hiders:
1. **Confirm Command** (`/confirm`): 
   - **Hider-only command** (seekers cannot use it)
   - During hide phase, marks hider as ready
   - Triggers zipper visual effect
   - Broadcasts "[Player] is ready!" message
   - When confirmed, hider is frozen until seek phase starts
   - If all hiders confirm early, transitions to seek phase immediately

2. **Item Boxes**: Hiders CAN pick up items from bonus boxes
   - Gives hiders defensive/evasive tools

3. **Elimination**: When found (hit by cake or bumped), hider is moved to lobby after configurable delay
   - Found time is recorded for result screen
   - Hiders can spectate remaining game or leave server from lobby

## Network/Online Play Features

### State Synchronization:
The mode includes comprehensive network state synchronization via:
- `saveCompleteState()`: Saves phase, timers, team assignments, confirmation status, found times, hint usage, etc.
- `restoreCompleteState()`: Restores complete game state for joining clients
- `getGameStartedProgress()`: Returns elapsed time and percentage of hiders found (for lobby display)

### Track Selection:
- Uses race tracks only (no arenas, soccer fields, or internal tracks)
- Implemented in `ServerLobby::updateTracksForMode()`

### Commands Available:

#### Player Commands:
1. `/confirm` - **Hiders only** - Mark ready during hide phase
2. `/hint` - **Seekers only** - Get distance hint to nearest hider
3. `/randomteams` - **All players** - Randomizes teams for hide and seek mode (no voting required)

#### Admin Commands (require `/power` to gain admin access):
4. `/poweruphint <number>` - Adjusts max hint uses for current round
5. `/sethidetime <seconds>` - Adjusts hide phase duration for next round
6. `/settotaltimecap <seconds>` - Adjusts total time cap for next round
7. `/setelimdelay <seconds>` - Adjusts elimination delay (time before found hiders are moved to lobby)

### Spectator Support:
- Found players are moved back to lobby (not directly to spectator mode)
- From lobby, they can choose to spectate remaining players or leave the server

### Live Join Handling:
- Can deny late joins with custom message via `m_hs_live_join_deny_message` config

## Configuration (ServerConfig)

Server administrators can configure via server_config.xml:
- `hide-time` (default 180s): Duration of hide phase
- `total-time-cap` (default 900s): Maximum game duration
- `fire-distance-threshold`: Max distance seekers can fire from
- `hot-threshold`: Distance threshold for "Hot" hint
- `warm-min` / `warm-max`: Distance range for "Warm" hint
- `hint-default-uses`: Default number of hints per seeker per round
- `hint-unlock-seconds`: Delay before hints become available
- `elimination-delay` (default 5.0s): Seconds before found hider is moved to lobby
- `live-join-deny-message`: Custom message when denying late joins

## GUI/Display Features

### During Race:
- Timer shows countdown from total time cap
- No position/rank display (not a race)
- Seekers: No minimap, no kart name tags visible
- Hiders: Normal minimap and display
- Team colors displayed in lobby with [HIDING]/[SEEKING] labels

### Result Screen:
- Shows which team won (Red/Blue)
- Timer display: `elapsed / total (remaining)`
- Two columns:
  - **Found**: List of found hiders with their found time (mm:ss.xx format)
  - **Remaining**: List of surviving hiders
- Similar styling to Soccer result screen

## Technical Implementation

### Key Classes:
1. **HideAndSeekWorld** (`src/modes/hide_seek_world.hpp/cpp`): Main game mode logic
2. **SmartCake** (`src/items/smart_cake.hpp/cpp`): Enhanced homing projectile for seekers
3. **Commands** (`src/lobby/commands/`): confirm, hint, poweruphint, sethidetime, settotaltimecap, setelimdelay, randomteams_hs

### Integration Points:
- **RaceManager**: Registered as `MINOR_MODE_HIDE_SEEK` / `IDENT_HIDE_SEEK`
- **ProjectileManager**: Creates SmartCake for seeker cakes
- **ItemManager**: Blocks seeker item collection
- **Powerup**: Distance gating and refund logic
- **Kart**: Team-based race result determination + collision detection for seeker bumping hider
- **RaceGUI**: Hides minimap for seekers
- **RaceResultGUI**: Custom result display
- **NetworkingLobby**: Team label display

## Bug Fixes Applied

### Bug Fix 1: Missing header and undeclared variable
**Issue**: In `src/items/powerup.cpp`, the variable `world` was used before declaration
**Fix**: 
- Moved `World *world = World::getWorld();` declaration before its first use
- Added missing include `#include "modes/hide_seek_world.hpp"`

### Bug Fix 2: Removed /found command
**Issue**: `/found` command is no longer needed since seekers must physically hit or use cakes
**Fix**: Removed `found.hpp`, `found.cpp`, and all references

### Bug Fix 3: Command permission enforcement
**Fixes Applied**:
- `/confirm`: Added check to ensure only hiders (Red team) can use it
- `/hint`: Already checked by `handleHintFor()` - only seekers can use it
- `/randomteams`: Changed to non-votable, allowing all players to use it without restrictions
- Admin commands: Already properly protected with `CMD_REQUIRE_PERM` checks

### Bug Fix 4: Physical collision detection
**Issue**: Seekers could not find hiders by bumping into them
**Fix**: Added Hide & Seek detection in `Kart::crashed()` to call `kartHit()` when a seeker bumps a hider

### Bug Fix 5: Configurable elimination delay
**Issue**: Elimination delay was hardcoded to 5.0 seconds
**Fix**: 
- Added `m_hs_elimination_delay` to ServerConfig (default 5.0s)
- Created `/setelimdelay` admin command
- Updated `kartHit()` to use the configurable value

## Online Playability Assessment

### ✅ Ready for Online Play:
1. **Complete network synchronization** via saveCompleteState/restoreCompleteState
2. **All game state properly serialized**: phases, timers, teams, confirmations, hints, cooldowns
3. **Broadcast messages** for game events (confirms, found notifications)
4. **Proper team assignment** via `/randomteams` command (available to all players)
5. **Lobby return for eliminated players** with choice to spectate or leave
6. **Progress tracking** for lobby display
7. **Server commands** for admin control and player actions
8. **Track filtering** properly configured
9. **Physical and projectile-based finding** both implemented
10. **Proper permission checks** on all commands

### Key Features:
1. **Two ways to find hiders**: Physical contact or smart cakes
2. **Team-specific commands**: `/confirm` for hiders, `/hint` for seekers
3. **Universal team randomization**: Any player can use `/randomteams`
4. **Admin controls**: Full set of admin commands for server management
5. **Configurable mechanics**: Elimination delay, hide time, time cap all adjustable
6. **Network-ready**: Full state synchronization and lobby management

## Conclusion
The Hide and Seek mode is **fully implemented and ready for online play**. All bugs have been fixed and the mode includes:
- Complete game logic for both phases
- Team-based mechanics with proper permission checks
- Physical and projectile-based finding mechanics
- Network synchronization
- Player commands (with proper restrictions)
- Admin controls
- Configurable elimination delay
- GUI integration
- Lobby return support for eliminated players

The mode is production-ready for online multiplayer playtesting.
