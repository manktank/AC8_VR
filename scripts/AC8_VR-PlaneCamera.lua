-- AC8 modular release.
-- AC8 modular local trial. Core is required; sibling features are optional.
-- Remove a feature file with the game closed; its native work is absent next launch.
local registry=rawget(_G,'AC8VRModules')
if not registry then registry={sections={},state='waiting',completed={}};rawset(_G,'AC8VRModules',registry)end

-- This feature's admission policy. Core supplies only demanded current-owner facts.
local function gameplay(R)
 return not R.settingsBusy('presentation','gameplay')and R.context.flight==true and R.viewTargetName():match('^BP_PlayerPlane_')~=nil and R.matchState()~='WaitingToStart'
end
assert(not registry.sections.gameplay_camera,'Duplicate gameplay camera')
registry.sections.gameplay_camera=function(R)
 local uevr=R.host('camera','gameplay camera stabilization')
local CAM_ANIM_KILL = true     
local CAM_KILL_EVERY = 1       
local function cam_local_pawn() return uevr.api:get_local_pawn(0) end
local function cam_player_pawn() return uevr.api:get_player_pawn() end
local function cam_index(o, k) return o[k] end
local function cam_invoke(o, k, ...) return o[k](o, ...) end
local function cam_player_controller(a)
    if a.get_player_controller ~= nil then return a:get_player_controller() end
end
local function cam_get_pawn()
    local ok, p = pcall(cam_local_pawn)
    if ok and p ~= nil then return p end
    local ok2, p2 = pcall(cam_player_pawn)
    if ok2 and p2 ~= nil then return p2 end
    return nil
end

local function cam_find_manager()
    local a = uevr and uevr.api
    if a == nil then return nil end
    local ctrl = nil
    local pawn = cam_get_pawn()
    if pawn ~= nil then
        local ok, v = pcall(cam_index, pawn, 'Controller'); if ok then ctrl = v end
        if ctrl == nil then
            ok, v = pcall(cam_invoke, pawn, 'get_Controller'); if ok then ctrl = v end
        end
    end
    if ctrl == nil then
        local ok, v = pcall(cam_player_controller, a); if ok then ctrl = v end
    end
    if ctrl == nil then return nil end
    local pm = nil
    local ok, v = pcall(cam_index, ctrl, 'PlayerCameraManager'); if ok then pm = v end
    if pm == nil then ok, v = pcall(cam_invoke, ctrl, 'get_PlayerCameraManager'); if ok then pm = v end end
    return pm
end

local function cam_kill_anims()
    local found, pm = pcall(cam_find_manager)
    if not found or pm == nil then return false end

    pcall(cam_invoke, pm, 'StopAllCameraAnims', true)
    pcall(cam_invoke, pm, 'StopAllCameraShakes', true)
    return true
end


 local frame_n=0
 uevr.sdk.callbacks.on_post_engine_tick(function()
  if not gameplay(R)or R.settingsBusy('camera','plane')then return end
  if registry.sections.inherited_hud and not R.gameplayCameraReady then return end
  local intent=rawget(_G,'AC8_CinematicIntent');frame_n=frame_n+1
    if CAM_ANIM_KILL and not (intent and intent.active) and frame_n % CAM_KILL_EVERY == 0 then
        cam_kill_anims()
    end
 end)
end

if registry.start then registry.start()end


