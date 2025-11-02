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
- **Seekers (Blue Team)**: Hunt down hiders using projectiles
- **End Condition**: Either all hiders are eliminated OR the total time cap is reached

## Win Conditions
- **Seekers Win**: If ALL hiders are found and eliminated before the total time cap expires
- **Hiders Win**: If ANY hider survives until the total time cap expires

## Teams
- **Red Team (KART_TEAM_RED)**: Hiders
- **Blue Team (KART_TEAM_BLUE)**: Seekers
- Teams are assigned before the race starts (likely via server commands like `/randomteams_hs`)

## Key Mechanics

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
   - Limited uses per round (configurable via `m_hs_hint_default_uses`)
   - 20-second cooldown between hints
   - Unlock time configurable (default 0 seconds from game start)
   - Returns distance category to nearest hider:
     - **Hot**: < `m_hs_hot_threshold` meters
     - **Warm**: Between `m_hs_warm_min` and `m_hs_warm_max` meters
     - **Cold**: Beyond warm range
   - If multiple hiders in a category, shows count

### For Hiders:
1. **Confirm Command** (`/confirm`): During hide phase, marks hider as ready
   - Triggers zipper visual effect
   - Broadcasts "[Player] is ready!" message
   - When confirmed, hider is frozen until seek phase starts
   - If all hiders confirm early, transitions to seek phase immediately

2. **Item Boxes**: Hiders CAN pick up items from bonus boxes
   - Gives hiders defensive/evasive tools

3. **Elimination**: When found (hit by cake), hider is eliminated after 5 seconds
   - Found time is recorded for result screen
   - Eliminated hiders are moved to spectator mode

### Manual Finding (`/found` command):
- Seeker can use `/found <player_name>` to manually mark a hider as found
- Requires proximity <= 5 meters
- Same 5-second elimination delay and effects as projectile hit

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
1. `/confirm` - Hiders mark themselves ready during hide phase
2. `/found <player>` - Seekers manually mark a hider as found (requires proximity)
3. `/hint` - Seekers get distance hint to nearest hider
4. `/poweruphint <number>` - Admin adjusts max hint uses for current round
5. `/sethidetime <seconds>` - Admin adjusts hide phase duration for next round
6. `/settotaltimecap <seconds>` - Admin adjusts total time cap for next round
7. `/randomteams_hs` - Randomizes teams for hide and seek mode

### Spectator Support:
- Found players are moved back to lobby in spectator mode
- Can spectate remaining players

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
3. **Commands** (`src/lobby/commands/`): confirm, found, hint, poweruphint, sethidetime, settotaltimecap, randomteams_hs

### Integration Points:
- **RaceManager**: Registered as `MINOR_MODE_HIDE_SEEK` / `IDENT_HIDE_SEEK`
- **ProjectileManager**: Creates SmartCake for seeker cakes
- **ItemManager**: Blocks seeker item collection
- **Powerup**: Distance gating and refund logic
- **Kart**: Team-based race result determination
- **RaceGUI**: Hides minimap for seekers
- **RaceResultGUI**: Custom result display
- **NetworkingLobby**: Team label display

## Bug Fix Applied
**Issue**: In `src/items/powerup.cpp`, the variable `world` was used before declaration
**Fix**: Moved `World *world = World::getWorld();` declaration before its first use
**Fix**: Added missing include `#include "modes/hide_seek_world.hpp"`

## Online Playability Assessment

### ✅ Ready for Online Play:
1. **Complete network synchronization** via saveCompleteState/restoreCompleteState
2. **All game state properly serialized**: phases, timers, teams, confirmations, hints, cooldowns
3. **Broadcast messages** for game events (confirms, found notifications)
4. **Proper team assignment** before game starts
5. **Spectator mode** for eliminated players
6. **Progress tracking** for lobby display
7. **Server commands** for admin control and player actions
8. **Track filtering** properly configured

### Potential Considerations:
1. **Team Balance**: Ensure mechanism for balanced team assignment (likely via `/randomteams_hs`)
2. **Late Joiners**: Configuration to deny/allow late joins is in place
3. **Network Latency**: Projectile hits are handled server-side via `kartHit()` override
4. **Rewind/Prediction**: Inherits from WorldWithRank which supports STK's rewind system

## Conclusion
The Hide and Seek mode is **fully implemented and ready for online play**. The codebase includes:
- Complete game logic for both phases
- Team-based mechanics
- Network synchronization
- Player commands
- Admin controls
- GUI integration
- Spectator support

The bug fix applied resolves a compilation error that would have prevented the mode from running. With this fix, the mode should be fully functional for online multiplayer playtesting.
