
------------------------------------------------------------------------
--  general.lua - General Soccer Predicates
--
--  Created: Sat Mar 14 17:43:05 2009
--  Copyright  2009  Tim Niemueller [www.niemueller.de]
--
--  $Id$
--
------------------------------------------------------------------------

--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation; either version 2 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU Library General Public License for more details.
--
--  Read the full text in the LICENSE.GPL file in the doc directory.

require("fawkes.predlib")

local math=math

--- This module provides a generic finite state machine (FSM).
-- @author Tim Niemueller
module(..., fawkes.predlib.module_init)

depends_interfaces = {
   {v="wm_ball", id="WM Ball", type="ObjectPositionInterface"},
   {v="gamestate", id="WM GameState", type="GameStateInterface"}
}

TEAM_NONE     = gamestate.TEAM_NONE
TEAM_CYAN     = gamestate.TEAM_CYAN
TEAM_MAGENTA  = gamestate.TEAM_MAGENTA
TEAM_BOTH     = gamestate.TEAM_BOTH

GOAL_BLUE     = gamestate.GOAL_BLUE
GOAL_YELLOW   = gamestate.GOAL_YELLOW

ROLE_GOALIE   = gamestate.ROLE_GOALIE
ROLE_DEFENDER = gamestate.ROLE_DEFENDER
ROLE_ATTACKER = gamestate.ROLE_ATTACKER

predparams = {
   ball_close_dist  = 0.18,
   ball_front_angle = 0.25,
   own_team = TEAM_CYAN,
   own_goal = GOAL_BLUE,
   own_role = ROLE_ATTACKER,
   ball_moved_min_dist = 0.1,
   ball_moved_ref_x = 0.0,
   ball_moved_ref_y = 0.0
}

function ball_visible()
   return wm_ball:is_valid() and wm_ball:is_visible()
end

function ball_close()
   return wm_ball:distance() < predparams.ball_close_dist
end

function ball_infront()
   return math.abs(wm_ball:bearing()) < predparams.ball_front_angle
end

function gamestate_frozen()
   return gamestate:game_state() == gamestate.GS_FROZEN
end

function gamestate_play()
   return gamestate:game_state() == gamestate.GS_PLAY
end

function gamestate_kick_off()
   return gamestate:game_state() == gamestate.GS_KICK_OFF
end

function gamestate_drop_ball()
   return gamestate:game_state() == gamestate.GS_DROP_BALL
end

function gamestate_penalty()
   return gamestate:game_state() == gamestate.GS_PENALTY
end

function gamestate_corner_kick()
   return gamestate:game_state() == gamestate.GS_CORNER_KICK
end

function gamestate_throw_in()
   return gamestate:game_state() == gamestate.GS_THROW_IN
end

function gamestate_free_kick()
   return gamestate:game_state() == gamestate.GS_FREE_KICK
end

function gamestate_goal_kick()
   return gamestate:game_state() == gamestate.GS_GOAL_KICK
end

function gamestate_half_time()
   return gamestate:game_state() == gamestate.GS_HALF_TIME
end

function gamestate_own_team()
   return gamestate:state_team() == predparams.own_team
end

function attacker()
   return gamestate:role() == ROLE_ATTACKER
end

function defender()
   return gamestate:role() == ROLE_DEFENDER
end

function goalie()
   return gamestate:role() == ROLE_GOALIE
end

function ball_moved()
   local dx = wm_ball:world_x() - ball_moved_ref_x
   local dy = wm_ball:world_y() - ball_moved_ref_y

   return math.sqrt(dx*dx + dy*dy) >= ball_moved_min_dist
end
