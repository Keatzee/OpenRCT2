#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "../common.h"
#include "../network/network.h"

extern "C"
{
    #include "../cheats.h"
    #include "../game.h"
    #include "../localisation/string_ids.h"
    #include "../ride/track.h"
    #include "../ride/track_data.h"
    #include "map.h"
    #include "park.h"
    #include "scenery.h"
}

/**
 * Gets whether the given track type can have a wall placed on the edge of the given direction.
 * Some thin tracks for example are allowed to have walls either side of the track, but wider tracks can not.
 */
static bool TrackIsAllowedWallEdges(uint8 rideType, uint8 trackType, uint8 trackSequence, uint8 direction)
{
    if (!ride_type_has_flag(rideType, RIDE_TYPE_FLAG_TRACK_NO_WALLS))
    {
        if (ride_type_has_flag(rideType, RIDE_TYPE_FLAG_FLAT_RIDE))
        {
            if (FlatRideTrackSequenceElementAllowedWallEdges[trackType][trackSequence] & (1 << direction))
            {
                return true;
            }
        }
        else
        {
            if (TrackSequenceElementAllowedWallEdges[trackType][trackSequence] & (1 << direction))
            {
                return true;
            }
        }
    }
    return false;
}

/**
 *
 *  rct2: 0x006E5CBA
 */
static bool WallCheckObstructionWithTrack(rct_scenery_entry * wall,
                                          sint32 x,
                                          sint32 y,
                                          sint32 z0,
                                          sint32 z1,
                                          sint32 edge,
                                          rct_map_element * trackElement,
                                          bool * wallAcrossTrack)
{
    sint32 trackType = trackElement->properties.track.type;
    sint32 sequence = map_element_get_track_sequence(trackElement);
    sint32 direction = (edge - map_element_get_direction(trackElement)) % 4;
    rct_ride * ride = get_ride(trackElement->properties.track.ride_index);

    if (TrackIsAllowedWallEdges(ride->type, trackType, sequence, direction))
    {
        return true;
    }

    if (!(wall->wall.flags & WALL_SCENERY_IS_DOOR))
    {
        return false;
    }

    // The following code checks if a door is allowed on the track
    if (!(RideData4[ride->type].flags & RIDE_TYPE_FLAG4_ALLOW_DOORS_ON_TRACK))
    {
        return false;
    }

    rct_ride_entry * rideEntry = get_ride_entry(ride->subtype);
    if (rideEntry->flags & RIDE_ENTRY_FLAG_16)
    {
        return false;
    }

    *wallAcrossTrack = true;
    if (z0 & 1)
    {
        return false;
    }

    sint32 z;
    if (sequence == 0)
    {
        if (TrackSequenceProperties[trackType][0] & TRACK_SEQUENCE_FLAG_DISALLOW_DOORS)
        {
            return false;
        }

        if (TrackDefinitions[trackType].bank_start == 0)
        {
            if (!(TrackCoordinates[trackType].rotation_begin & 4))
            {
                direction = map_element_get_direction_with_offset(trackElement, 2);
                if (direction == edge)
                {
                    const rct_preview_track * trackBlock = &TrackBlocks[trackType][sequence];
                    z = TrackCoordinates[trackType].z_begin;
                    z = trackElement->base_height + ((z - trackBlock->z) * 8);
                    if (z == z0)
                    {
                        return true;
                    }
                }
            }
        }
    }

    const rct_preview_track * trackBlock = &TrackBlocks[trackType][sequence + 1];
    if (trackBlock->index != 0xFF)
    {
        return false;
    }

    if (TrackDefinitions[trackType].bank_end != 0)
    {
        return false;
    }

    direction = TrackCoordinates[trackType].rotation_end;
    if (direction & 4)
    {
        return false;
    }

    direction = map_element_get_direction(trackElement);
    if (direction != edge)
    {
        return false;
    }

    trackBlock = &TrackBlocks[trackType][sequence];
    z = TrackCoordinates[trackType].z_end;
    z = trackElement->base_height + ((z - trackBlock->z) * 8);
    if (z != z0)
    {
        return false;
    }

    return true;
}

/**
 *
 *  rct2: 0x006E5C1A
 */
static bool WallCheckObstruction(rct_scenery_entry * wall,
                                 sint32 x,
                                 sint32 y,
                                 sint32 z0,
                                 sint32 z1,
                                 sint32 edge,
                                 bool * wallAcrossTrack)
{
    sint32 entryType, sequence;
    rct_scenery_entry * entry;
    rct_large_scenery_tile * tile;

    *wallAcrossTrack = false;
    gMapGroundFlags = ELEMENT_IS_ABOVE_GROUND;
    if (map_is_location_at_edge(x, y))
    {
        gGameCommandErrorText = STR_OFF_EDGE_OF_MAP;
        return false;
    }

    rct_map_element * mapElement = map_get_first_element_at(x / 32, y / 32);
    do
    {
        sint32 elementType = map_element_get_type(mapElement);
        if (elementType == MAP_ELEMENT_TYPE_SURFACE) continue;
        if (z0 >= mapElement->clearance_height) continue;
        if (z1 <= mapElement->base_height) continue;
        if (elementType == MAP_ELEMENT_TYPE_WALL)
        {
            sint32 direction = map_element_get_direction(mapElement);
            if (edge == direction)
            {
                map_obstruction_set_error_text(mapElement);
                return false;
            }
            continue;
        }
        if ((mapElement->flags & 0x0F) == 0) continue;

        switch (elementType) {
        case MAP_ELEMENT_TYPE_ENTRANCE:
            map_obstruction_set_error_text(mapElement);
            return false;
        case MAP_ELEMENT_TYPE_PATH:
            if (mapElement->properties.path.edges & (1 << edge))
            {
                map_obstruction_set_error_text(mapElement);
                return false;
            }
            break;
        case MAP_ELEMENT_TYPE_SCENERY_MULTIPLE:
            entryType = mapElement->properties.scenerymultiple.type & 0x3FF;
            sequence = mapElement->properties.scenerymultiple.type >> 10;
            entry = get_large_scenery_entry(entryType);
            tile = &entry->large_scenery.tiles[sequence];
            {
                sint32 direction = ((edge - map_element_get_direction(mapElement)) % 4) + 8;
                if (!(tile->var_7 & (1 << direction)))
                {
                    map_obstruction_set_error_text(mapElement);
                    return false;
                }
            }
            break;
        case MAP_ELEMENT_TYPE_SCENERY:
            entryType = mapElement->properties.scenery.type;
            entry = get_small_scenery_entry(entryType);
            if (entry->small_scenery.flags & SMALL_SCENERY_FLAG_ALLOW_WALLS)
            {
                map_obstruction_set_error_text(mapElement);
                return false;
            }
            break;
        case MAP_ELEMENT_TYPE_TRACK:
            if (!WallCheckObstructionWithTrack(wall, x, y, z0, z1, edge, mapElement, wallAcrossTrack))
            {
                return false;
            }
            break;
        }
    }
    while (!map_element_is_last_for_tile(mapElement++));

    return true;
}

#pragma region Edge Slopes Table

enum EDGE_SLOPE
{
    EDGE_SLOPE_ELEVATED     = (1 << 0), // 0x01
    EDGE_SLOPE_UPWARDS      = (1 << 6), // 0x40
    EDGE_SLOPE_DOWNWARDS    = (1 << 7), // 0x80

    EDGE_SLOPE_UPWARDS_ELEVATED     = EDGE_SLOPE_UPWARDS | EDGE_SLOPE_ELEVATED,
    EDGE_SLOPE_DOWNWARDS_ELEVATED   = EDGE_SLOPE_DOWNWARDS | EDGE_SLOPE_ELEVATED,
};

/** rct2: 0x009A3FEC */
static const uint8 EdgeSlopes[][4] = {
//  Top right                        Bottom right                   Bottom left                       Top left
    { 0,                             0,                             0,                             0                             },
    { 0,                             EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_DOWNWARDS,          0                             },
    { 0,                             0,                             EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_DOWNWARDS          },
    { 0,                             EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_DOWNWARDS          },
    { EDGE_SLOPE_DOWNWARDS,          0,                             0,                             EDGE_SLOPE_UPWARDS            },
    { EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS            },
    { EDGE_SLOPE_DOWNWARDS,          0,                             EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_ELEVATED           },
    { EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_ELEVATED           },
    { EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_DOWNWARDS,          0,                             0                             },
    { EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_DOWNWARDS,          0                             },
    { EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_DOWNWARDS          },
    { EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_DOWNWARDS          },
    { EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_DOWNWARDS,          0,                             EDGE_SLOPE_UPWARDS            },
    { EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS            },
    { EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_ELEVATED           },
    { EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_ELEVATED,           EDGE_SLOPE_ELEVATED           },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_UPWARDS_ELEVATED,   EDGE_SLOPE_DOWNWARDS_ELEVATED },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { 0,                             0,                             0,                             0                             },
    { EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_UPWARDS_ELEVATED,   EDGE_SLOPE_DOWNWARDS_ELEVATED, EDGE_SLOPE_DOWNWARDS          },
    { 0,                             0,                             0,                             0                             },
    { EDGE_SLOPE_UPWARDS_ELEVATED,   EDGE_SLOPE_DOWNWARDS_ELEVATED, EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS            },
    { EDGE_SLOPE_DOWNWARDS_ELEVATED, EDGE_SLOPE_DOWNWARDS,          EDGE_SLOPE_UPWARDS,            EDGE_SLOPE_UPWARDS_ELEVATED   },
    { 0,                             0,                             0,                             0                             },
};

#pragma endregion

static money32 WallPlace(uint8 wallType,
                  sint16 x,
                  sint16 y,
                  sint16 z,
                  uint8 edge,
                  uint8 primaryColour,
                  uint8 secondaryColour,
                  uint8 tertiaryColour,
                  uint8 flags)
{
    rct_xyz16 position = { x, y, z };

    gCommandExpenditureType = RCT_EXPENDITURE_TYPE_LANDSCAPING;
    gCommandPosition.x = position.x + 16;
    gCommandPosition.y = position.y + 16;
    gCommandPosition.z = position.z;

    if (position.z == 0)
    {
        gCommandPosition.z = map_element_height(position.x, position.y) & 0xFFFF;
    }

    if (game_is_paused() && !gCheatsBuildInPauseMode)
    {
        gGameCommandErrorText = STR_CONSTRUCTION_NOT_POSSIBLE_WHILE_GAME_IS_PAUSED;
        return MONEY32_UNDEFINED;
    }

    if (!(gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) &&
        !(flags & GAME_COMMAND_FLAG_PATH_SCENERY) &&
        !gCheatsSandboxMode)
    {

        if (position.z == 0)
        {
            if (!map_is_location_in_park(position.x, position.y))
            {
                return MONEY32_UNDEFINED;
            }
        }
        else if (!map_is_location_owned(position.x, position.y, position.z))
        {
            return MONEY32_UNDEFINED;
        }
    }

    uint8 edgeSlope = 0;
    if (position.z == 0)
    {
        rct_map_element * surfaceElement = map_get_surface_element_at(position.x / 32, position.y / 32);
        if (surfaceElement == nullptr)
        {
            return MONEY32_UNDEFINED;
        }
        position.z = surfaceElement->base_height * 8;

        uint8 slope = surfaceElement->properties.surface.slope & MAP_ELEMENT_SLOPE_MASK;
        edgeSlope = EdgeSlopes[slope][edge & 3];
        if (edgeSlope & EDGE_SLOPE_ELEVATED)
        {
            position.z += 16;
            edgeSlope &= ~EDGE_SLOPE_ELEVATED;
        }
    }

    rct_map_element * surfaceElement = map_get_surface_element_at(position.x / 32, position.y / 32);
    if (surfaceElement == nullptr)
    {
        return MONEY32_UNDEFINED;
    }

    if (map_get_water_height(surfaceElement) > 0)
    {
        uint16 waterHeight = map_get_water_height(surfaceElement) * 16;

        if (position.z < waterHeight && !gCheatsDisableClearanceChecks)
        {
            gGameCommandErrorText = STR_CANT_BUILD_THIS_UNDERWATER;
            return MONEY32_UNDEFINED;
        }
    }

    if (position.z / 8 < surfaceElement->base_height && !gCheatsDisableClearanceChecks)
    {
        gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ABOVE_GROUND;
        return MONEY32_UNDEFINED;
    }

    if (!(edgeSlope & (EDGE_SLOPE_UPWARDS | EDGE_SLOPE_DOWNWARDS)))
    {
        uint8 newEdge = (edge + 2) & 3;
        uint8 newBaseHeight = surfaceElement->base_height;
        newBaseHeight += 2;
        if (surfaceElement->properties.surface.slope & (1 << newEdge))
        {
            if (position.z / 8 < newBaseHeight)
            {
                gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ABOVE_GROUND;
                return MONEY32_UNDEFINED;
            }

            if (surfaceElement->properties.surface.slope & (1 << 4))
            {
                newEdge = (newEdge - 1) & 3;

                if (surfaceElement->properties.surface.slope & (1 << newEdge))
                {
                    newEdge = (newEdge + 2) & 3;
                    if (surfaceElement->properties.surface.slope & (1 << newEdge))
                    {
                        newBaseHeight += 2;
                        if (position.z / 8 < newBaseHeight)
                        {
                            gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ABOVE_GROUND;
                            return MONEY32_UNDEFINED;
                        }
                        newBaseHeight -= 2;
                    }
                }
            }
        }

        newEdge = (edge + 3) & 3;
        if (surfaceElement->properties.surface.slope & (1 << newEdge))
        {
            if (position.z / 8 < newBaseHeight)
            {
                gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ABOVE_GROUND;
                return MONEY32_UNDEFINED;
            }

            if (surfaceElement->properties.surface.slope & (1 << 4))
            {
                newEdge = (newEdge - 1) & 3;

                if (surfaceElement->properties.surface.slope & (1 << newEdge))
                {
                    newEdge = (newEdge + 2) & 3;
                    if (surfaceElement->properties.surface.slope & (1 << newEdge))
                    {
                        newBaseHeight += 2;
                        if (position.z / 8 < newBaseHeight)
                        {
                            gGameCommandErrorText = STR_CAN_ONLY_BUILD_THIS_ABOVE_GROUND;
                            return MONEY32_UNDEFINED;
                        }
                    }
                }
            }
        }
    }
    sint32 bannerIndex = 0xFF;
    rct_scenery_entry * wallEntry = get_wall_entry(wallType);
    // Have to check both -1 and nullptr, as one can be a invalid object,
    // while the other can be invalid index
    if ((uintptr_t)wallEntry == (uintptr_t)-1 || wallEntry == nullptr)
    {
        return MONEY32_UNDEFINED;
    }

    if (wallEntry->wall.scrolling_mode != 0xFF)
    {
        bannerIndex = create_new_banner(flags);

        if (bannerIndex == 0xFF)
        {
            return MONEY32_UNDEFINED;
        }

        rct_banner * banner = &gBanners[bannerIndex];
        if (flags & GAME_COMMAND_FLAG_APPLY)
        {
            banner->flags |= BANNER_FLAG_IS_WALL;
            banner->type = 0;
            banner->x = position.x / 32;
            banner->y = position.y / 32;

            sint32 rideIndex = banner_get_closest_ride_index(position.x, position.y, position.z);
            if (rideIndex != -1)
            {
                banner->colour = rideIndex & 0xFF;
                banner->flags |= BANNER_FLAG_LINKED_TO_RIDE;
            }
        }
    }

    uint8 clearanceHeight = position.z / 8;
    if (edgeSlope & (EDGE_SLOPE_UPWARDS | EDGE_SLOPE_DOWNWARDS))
    {
        if (wallEntry->wall.flags & WALL_SCENERY_CANT_BUILD_ON_SLOPE)
        {
            gGameCommandErrorText = STR_ERR_UNABLE_TO_BUILD_THIS_ON_SLOPE;
            return MONEY32_UNDEFINED;
        }
        clearanceHeight += 2;
    }
    clearanceHeight += wallEntry->wall.height;

    bool wallAcrossTrack = false;
    if (!(flags & GAME_COMMAND_FLAG_PATH_SCENERY) && !gCheatsDisableClearanceChecks)
    {
        if (!WallCheckObstruction(wallEntry,
                                  position.x,
                                  position.y,
                                  position.z / 8,
                                  clearanceHeight,
                                  edge,
                                  &wallAcrossTrack))
        {
            return MONEY32_UNDEFINED;
        }
    }

    if (!map_check_free_elements_and_reorganise(1))
    {
        return MONEY32_UNDEFINED;
    }

    if (flags & GAME_COMMAND_FLAG_APPLY)
    {
        if (gGameCommandNestLevel == 1 && !(flags & GAME_COMMAND_FLAG_GHOST))
        {
            rct_xyz16 coord;
            coord.x = position.x + 16;
            coord.y = position.y + 16;
            coord.z = map_element_height(coord.x, coord.y);
            network_set_player_last_action_coord(network_get_player_index(game_command_playerid), coord);
        }

        rct_map_element * mapElement = map_element_insert(position.x / 32, position.y / 32, position.z / 8, 0);
        assert(mapElement != nullptr);

        map_animation_create(MAP_ANIMATION_TYPE_WALL, position.x, position.y, position.z / 8);

        mapElement->clearance_height = clearanceHeight;

        mapElement->type = edgeSlope | edge | MAP_ELEMENT_TYPE_WALL;

        mapElement->properties.wall.colour_1 = primaryColour;
        wall_element_set_secondary_colour(mapElement, secondaryColour);

        if (wallAcrossTrack)
        {
            mapElement->properties.wall.animation |= WALL_ANIMATION_FLAG_ACROSS_TRACK;
        }

        mapElement->properties.wall.type = wallType;
        if (bannerIndex != 0xFF)
        {
            mapElement->properties.wall.banner_index = bannerIndex;
        }

        if (wallEntry->wall.flags & WALL_SCENERY_HAS_TERNARY_COLOUR)
        {
            mapElement->properties.wall.colour_3 = tertiaryColour;
        }

        if (flags & GAME_COMMAND_FLAG_GHOST)
        {
            mapElement->flags |= MAP_ELEMENT_FLAG_GHOST;
        }

        gSceneryMapElement = mapElement;
        map_invalidate_tile_zoom1(position.x, position.y, mapElement->base_height * 8, mapElement->base_height * 8 + 72);
    }

    if (gParkFlags & PARK_FLAGS_NO_MONEY)
    {
        return 0;
    }
    else
    {
        return wallEntry->wall.price;
    }
}

static rct_map_element * GetFirstWallElementAt(sint32 x, sint32 y, uint8 baseZ, uint8 direction, bool isGhost)
{
    rct_map_element * mapElement = map_get_first_element_at(x / 32, y / 32);
    do
    {
        if (map_element_get_type(mapElement) != MAP_ELEMENT_TYPE_WALL) continue;
        if (mapElement->base_height != baseZ) continue;
        if ((map_element_get_direction(mapElement)) != direction) continue;
        if (map_element_is_ghost(mapElement) != isGhost) continue;
        return mapElement;
    }
    while (!map_element_is_last_for_tile(mapElement++));
    return nullptr;
}

static money32 WallRemove(sint16 x, sint16 y, uint8 baseHeight, uint8 direction, uint8 flags)
{
    if (!map_is_location_valid(x, y))
    {
        return MONEY32_UNDEFINED;
    }

    gCommandExpenditureType = RCT_EXPENDITURE_TYPE_LANDSCAPING;
    bool isGhost = (flags & GAME_COMMAND_FLAG_GHOST) != 0;
    if (!isGhost &&
        game_is_paused() &&
        !gCheatsBuildInPauseMode)
    {
        gGameCommandErrorText = STR_CONSTRUCTION_NOT_POSSIBLE_WHILE_GAME_IS_PAUSED;
        return MONEY32_UNDEFINED;
    }

    if (!isGhost &&
        !(gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) &&
        !gCheatsSandboxMode &&
        !map_is_location_owned(x, y, baseHeight * 8))
    {
        return MONEY32_UNDEFINED;
    }

    rct_map_element * wallElement = GetFirstWallElementAt(x, y, baseHeight, direction, isGhost);
    if (!(flags & GAME_COMMAND_FLAG_APPLY) || (wallElement == nullptr))
    {
        return 0;
    }

    if (gGameCommandNestLevel == 1 && !isGhost)
    {
        rct_xyz16 coord;
        coord.x = x + 16;
        coord.y = y + 16;
        coord.z = map_element_height(coord.x, coord.y);
        network_set_player_last_action_coord(network_get_player_index(game_command_playerid), coord);
    }

    map_element_remove_banner_entry(wallElement);
    map_invalidate_tile_zoom1(x, y, wallElement->base_height * 8, (wallElement->base_height * 8) + 72);
    map_element_remove(wallElement);
    return 0;
}

static money32 WallSetColour(sint16 x,
                             sint16 y,
                             uint8 baseHeight,
                             uint8 direction,
                             uint8 primaryColour,
                             uint8 secondaryColour,
                             uint8 tertiaryColour,
                             uint8 flags)
{
    gCommandExpenditureType = RCT_EXPENDITURE_TYPE_LANDSCAPING;
    sint32 z = baseHeight * 8;

    gCommandPosition.x = x + 16;
    gCommandPosition.y = y + 16;
    gCommandPosition.z = z;

    if (!(gScreenFlags & SCREEN_FLAGS_SCENARIO_EDITOR) &&
        !map_is_location_in_park(x, y) &&
        !gCheatsSandboxMode)
    {

        return MONEY32_UNDEFINED;
    }

    rct_map_element * wallElement = map_get_wall_element_at(x, y, baseHeight, direction);
    if (wallElement == nullptr)
    {
        return 0;
    }

    if ((flags & GAME_COMMAND_FLAG_GHOST) && !(wallElement->flags & MAP_ELEMENT_FLAG_GHOST))
    {
        return 0;
    }

    if (flags & GAME_COMMAND_FLAG_APPLY)
    {
        rct_scenery_entry * scenery_entry = get_wall_entry(wallElement->properties.wall.type);
        wallElement->properties.wall.colour_1 = primaryColour;
        wall_element_set_secondary_colour(wallElement, secondaryColour);

        if (scenery_entry->wall.flags & WALL_SCENERY_HAS_TERNARY_COLOUR)
        {
            wallElement->properties.wall.colour_3 = tertiaryColour;
        }
        map_invalidate_tile_zoom1(x, y, z, z + 72);
    }

    return 0;
}

extern "C"
{
    uint8 wall_element_get_animation_frame(rct_map_element * wallElement)
    {
        return (wallElement->properties.wall.animation >> 3) & 0xF;
    }

    void wall_element_set_animation_frame(rct_map_element * wallElement, uint8 frameNum)
    {
        wallElement->properties.wall.animation &= WALL_ANIMATION_FLAG_ALL_FLAGS;
        wallElement->properties.wall.animation |= (frameNum & 0xF) << 3;
    }

    uint8 wall_element_get_secondary_colour(rct_map_element * wallElement)
    {
        uint8 secondaryColour = (wallElement->properties.wall.colour_1 & 0xE0) >> 5;
        secondaryColour |= (wallElement->flags & 0x60) >> 2;
        return secondaryColour;
    }

    void wall_element_set_secondary_colour(rct_map_element * wallElement, uint8 secondaryColour)
    {
        wallElement->properties.wall.colour_1 &= 0x1F;
        wallElement->properties.wall.colour_1 |= (secondaryColour & 0x7) << 5;
        wallElement->flags &= 0x9F;
        wallElement->flags |= (secondaryColour & 0x18) << 2;
    }

    /**
     *
     *  rct2: 0x006E588E
     */
    void wall_remove_at(sint32 x, sint32 y, sint32 z0, sint32 z1)
    {
        rct_map_element * mapElement;

        z0 /= 8;
        z1 /= 8;
    repeat:
        mapElement = map_get_first_element_at(x >> 5, y >> 5);
        do
        {
            if (map_element_get_type(mapElement) != MAP_ELEMENT_TYPE_WALL)
                continue;
            if (z0 >= mapElement->clearance_height)
                continue;
            if (z1 <= mapElement->base_height)
                continue;

            map_element_remove_banner_entry(mapElement);
            map_invalidate_tile_zoom1(x, y, mapElement->base_height * 8, mapElement->base_height * 8 + 72);
            map_element_remove(mapElement);
            goto repeat;
        }
        while (!map_element_is_last_for_tile(mapElement++));
    }

    /**
     *
     *  rct2: 0x006E57E6
     */
    void wall_remove_at_z(sint32 x, sint32 y, sint32 z)
    {
        wall_remove_at(x, y, z, z + 48);
    }

    /**
     *
     *  rct2: 0x006E5935
     */
    void wall_remove_intersecting_walls(sint32 x, sint32 y, sint32 z0, sint32 z1, sint32 direction)
    {
        rct_map_element * mapElement;

        mapElement = map_get_first_element_at(x >> 5, y >> 5);
        do
        {
            if (map_element_get_type(mapElement) != MAP_ELEMENT_TYPE_WALL)
                continue;

            if (mapElement->clearance_height <= z0 || mapElement->base_height >= z1)
                continue;

            if (direction != map_element_get_direction(mapElement))
                continue;

            map_element_remove_banner_entry(mapElement);
            map_invalidate_tile_zoom1(x, y, mapElement->base_height * 8, mapElement->base_height * 8 + 72);
            map_element_remove(mapElement);
            mapElement--;
        }
        while (!map_element_is_last_for_tile(mapElement++));
    }

    /**
     *
     *  rct2: 0x006E519A
     */
    void game_command_place_wall(sint32 * eax,
                                 sint32 * ebx,
                                 sint32 * ecx,
                                 sint32 * edx,
                                 sint32 * esi,
                                 sint32 * edi,
                                 sint32 * ebp)
    {
        *ebx = WallPlace(
            (*ebx >> 8) & 0xFF,
            *eax & 0xFFFF,
            *ecx & 0xFFFF,
            *edi & 0xFFFF,
            *edx & 0xFF,
            (*edx >> 8) & 0xFF,
            *ebp & 0xFF,
            (*ebp >> 8) & 0xFF,
            *ebx & 0xFF
        );
    }

    money32 wall_place(sint32 type,
                       sint32 x,
                       sint32 y,
                       sint32 z,
                       sint32 edge,
                       sint32 primaryColour,
                       sint32 secondaryColour,
                       sint32 tertiaryColour,
                       sint32 flags)
    {
        sint32 eax = x;
        sint32 ebx = flags | (type << 8);
        sint32 ecx = y;
        sint32 edx = edge | (primaryColour << 8);
        sint32 esi = 0;
        sint32 edi = z;
        sint32 ebp = secondaryColour | (tertiaryColour << 8);
        game_command_place_wall(&eax, &ebx, &ecx, &edx, &esi, &edi, &ebp);
        return ebx;
    }

    /**
     *
     *  rct2: 0x006E5597
     */
    void game_command_remove_wall(sint32 * eax,
                                  sint32 * ebx,
                                  sint32 * ecx,
                                  sint32 * edx,
                                  sint32 * esi,
                                  sint32 * edi,
                                  sint32 * ebp)
    {
        *ebx = WallRemove(
            *eax & 0xFFFF,
            *ecx & 0xFFFF,
            (*edx >> 8) & 0xFF,
            *edx & 0xFF,
            *ebx & 0xFF
        );
    }

    /**
     *
     *  rct2: 0x006E56B5
     */
    void game_command_set_wall_colour(sint32 * eax,
                                      sint32 * ebx,
                                      sint32 * ecx,
                                      sint32 * edx,
                                      sint32 * esi,
                                      sint32 * edi,
                                      sint32 * ebp)
    {
        *ebx = WallSetColour(
            *eax & 0xFFFF,
            *ecx & 0xFFFF,
            (*edx >> 8) & 0xFF,
            *edx & 0xFF,
            (*ebx >> 8) & 0xFF,
            *ebp & 0xFF,
            (*ebp >> 8) & 0xFF,
            *ebx & 0xFF
        );
    }
}
