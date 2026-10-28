-- AC8 modular release.
-- AC8 modular local trial. Core is required; sibling features are optional.
-- Remove a feature file with the game closed; its native work is absent next launch.
local registry=rawget(_G,'AC8VRModules')
if not registry then registry={sections={},state='waiting',completed={}};rawset(_G,'AC8VRModules',registry)end

-- This feature's admission policy. Core supplies only demanded current-owner facts.
local function movie(R)
 if R.settingsBusy('presentation','gameplay')then return true end
 local c=R.context;local hn=c.hudName or'';local tn=R.viewTargetName()
 return hn:match('^BP_LiveCinemaHUD_C ')~=nil or hn:match('^BP_LiveHangarHUD_C ')~=nil or
  tn:match('^BP_MGC_CineCamera_C ')~=nil or tn:match('^BP_IGC_CameraPawn_')~=nil
end
local function gameplay(R)
 return not R.settingsBusy('presentation','gameplay')and R.context.flight==true and R.viewTargetName():match('^BP_PlayerPlane_')~=nil and R.matchState()~='WaitingToStart'
end
assert(not registry.sections['mission_loading'], 'Duplicate AC8 VR section: mission_loading')
registry.sections['mission_loading'] = function(R)
do
-- V85: the current loading subsystem owns the actual mission-loading screen.
-- Captured native UI is not in the viewport. The persistent status-icon root
-- alone must never count as loading. No widget setters or pixel tests.
local R=AC8Runtime
local S={version=85,nextScan=0,active=false,reason='waiting'}
R.missionLoading=S
local function call(o,k,...)
 if not o or(o~=uevr.api and not R.isLiveObject(o))then return end
 local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end
end
local function full(o)return call(o,'get_full_name')end
local function prop(o,k)return call(o,'get_property',k)end
local function clear(reason)S.active=false;S.screen=nil;S.screenName=nil;S.reason=reason end
local function visible(o)
 local v=call(o,'GetVisibility');local a=call(o,'GetRenderOpacity')
 return type(v)=='number'and v~=1 and v~=2 and type(a)=='number'and a==a and a>0
end
local statics
local function update()
 -- The transition watcher already observes MatchState each pre/post tick.
 -- Reuse its same-HUD InProgress result instead of polling loading widgets.
 -- Keep following an already visible loading screen until it actually clears.
 -- Missing/stale watcher state retains the original ownership check.
 local tr=rawget(_G,'AC8VRTransitions');local c=R.context
 if c.flight and not S.active and R.matchState()=='InProgress'then
  S.reason='idle during confirmed flight';return
 end
 if not R.isLiveObject(statics)then statics=call(uevr.api,'find_uobject','GameplayStatics /Script/Engine.Default__GameplayStatics')end
 local gi=call(statics,'GetGameInstance',R.context.pc);local gin=full(gi)
 if not gin then S.subsystem=nil;S.gameInstance=nil;S.nextScan=0;clear('no current game instance');return end
 if gin~=S.gameInstance then S.gameInstance=gin;S.subsystem=nil;S.nextScan=0 end
 if S.subsystem and (not R.isLiveObject(S.subsystem)or full(call(S.subsystem,'get_outer'))~=gin)then S.subsystem=nil;S.nextScan=0 end
 if not S.subsystem and R.tick>=S.nextScan then
  S.nextScan=R.tick+60
  local cls=call(uevr.api,'find_uobject','Class /Script/Live.LiveLoadingScreenSubsystem')
  for i,o in ipairs(call(cls,'get_objects_matching',false)or{})do
   if i>8 then break end
   local n=full(o)
   if n and n:match('^LiveLoadingScreenSubsystem ')and not n:find('Default__',1,true)and full(call(o,'get_outer'))==gin then S.subsystem=o;break end
  end
 end
 if not S.subsystem then clear('no current loading subsystem');return end
 local root=prop(S.subsystem,'LoadingScreenRootWidget')
 if not(full(root)or''):match('^WBP_Loading_Root_000_C ')or full(call(root,'get_outer'))~=gin then clear('no owned loading root');return end
 local screen=prop(root,'LoadingScreen');local name=full(screen)
 -- Narrow to the observed mission manager; startup, save/connect and other
 -- loading-screen types keep their existing behavior.
 if not(name or''):match('^WBP_MissionLoad_ManagerWidget_000_C ')or full(call(screen,'get_outer'))~=gin then clear('no mission loading screen');return end
 local panel=prop(root,'RootPanel');local panelName=full(panel)
 if not panelName or full(call(screen,'GetParent'))~=panelName then clear('mission screen detached');return end
 local current=screen;local seen={}
 for _=1,16 do
  local n=full(current)
  if not n or seen[n]or not visible(current)then clear('mission screen hidden or stale');return end
  seen[n]=true
  if n==full(root)then S.active=true;S.screen=screen;S.screenName=name;S.reason='owned visible mission loading screen';return end
  local parent=call(current,'GetParent')
  if not parent then
   local outer=call(current,'get_outer')
   if(full(outer)or''):match('^WidgetTree ')then parent=call(outer,'get_outer')end
  end
  if not parent then clear('mission screen chain detached');return end
  local selected=call(parent,'GetActiveWidget')
  if selected and full(selected)~=n then clear('inactive loading branch');return end
  current=parent
 end
 clear('loading hierarchy too deep')
end
local host=R.host('utility','mission loading ownership')
host.sdk.callbacks.on_pre_engine_tick(update)
host.sdk.callbacks.on_post_engine_tick(update)
end


end
assert(not registry.sections.transitions,'Duplicate AC8 VR section: transitions')
registry.sections.transitions=function(R)
(function(uevr,json,print)
local api=uevr.api
local old=rawget(_G,'AC8VRTransitions');if old then old.stopped=true end
local S={version=88,binaryCover=true,stopped=false,ticks=0,elapsed=0,loading=false,opacity=0,
    customFadesDisabled=true,source='custom fades disabled',weaponMenus={}}
rawset(_G,'AC8VRTransitions',S)
local function call(o,k,...)
    if not o or(o~=api and not AC8Runtime.isLiveObject(o))then return end
    local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end
end
local function full(o)return call(o,'get_full_name')end
local function prop(o,k)return call(o,'get_property',k)end
local gameStateStatics
local function stateValue(state)
    local v=prop(state,'MatchState');S.matchStateValueType=type(v)
    if type(v)=='string'then return v end
    -- FName is a reflected value, not a UObject tracked by UObjectHook.exists.
    -- Convert the value directly; the owning GameState was already validated.
    if v~=nil then
        local ok,text=pcall(function()return v:to_string()end)
        if ok and type(text)=='string'then return text end
    end
    

end
local function matchState(pc)
    local level=(full(pc) or ''):match('%s(.+%.PersistentLevel)%.')
    if not level then return nil end
    local shared=R.matchState()
    if shared~=nil then return shared end
    -- Cache only the static helper, never the world's selected GameState.
    -- Ask the current world each pre/post tick so same-world replacements and
    -- WaitingToStart changes are observed immediately, even if old actors live.
    if not AC8Runtime.isLiveObject(gameStateStatics)then
        gameStateStatics=call(api,'find_uobject','GameplayStatics /Script/Engine.Default__GameplayStatics')
    end
    local current=call(gameStateStatics,'GetGameState',pc)
    local name=full(current)or''
    if name:match('^LiveGameState ')and name:find(' '..level..'.',1,true)then
        local value=stateValue(current)
        if value~=nil then
            
            return value
        end
    end
    -- Preserve the previous discovery behavior when reflection, type, owner or
    -- MatchState is unavailable. Do not cache absence or delay a later retry.
    
    for _,state in ipairs(call(call(api,'find_uobject','Class /Script/Live.LiveGameState'),'get_objects_matching',false) or {})do
        if (full(state) or ''):find(level..'.',1,true)then
            return stateValue(state)
        end
    end
end

local function clearCover()
    call(api,'dispatch_custom_event','AC8_NativeFadeSource','0|1')
    call(api,'dispatch_custom_event','AC8_TransitionCover','0|0|0|0|0|0|0|1|1')
    S.coverPacket='0|0|0|0|0|0|0|1|1'
end
clearCover()
local function update(_,delta)
    if S.stopped then return end
    S.ticks=S.ticks+1;S.elapsed=S.elapsed+math.max(0,math.min(tonumber(delta)or 0,1))
    local c=AC8Runtime.context;local hn=c.hudName or''
    local state=c.flight and matchState(c.pc)or nil
    S.hud=hn;S.matchState=state
    local rawLoading=hn:find('/Game/Maps/Transition/',1,true)~=nil or
        (c.flight and state=='WaitingToStart')or AC8Runtime.missionLoading.active==true
    local menu=hn:match('^BP_LiveMenuHUD_C ')~=nil
    if rawLoading then S.awaitingDestination=true end
    local ci=rawget(_G,'AC8_CinematicIntent')
    local layoutReady=not AC8Runtime.layoutEnabled or
        (AC8Runtime.gameplayCameraReady==true and AC8Runtime.gameplayCameraReadyEpoch==c.epoch)
    local flightReady=c.flight and state=='InProgress'and layoutReady and not AC8Runtime.settingsBusy('presentation','gameplay')
    local movieReady=ci and (ci.maskActive==true or ci.hangarActive==true)
    -- A missing optional cinematic module must never keep an invisible owner alive.
    if not ci then movieReady=hn:match('^BP_LiveCinemaHUD_C ')~=nil or hn:match('^BP_LiveHangarHUD_C ')~=nil end
    if not rawLoading and (menu or flightReady or movieReady)then S.awaitingDestination=false end
    S.loading=rawLoading or S.awaitingDestination==true
    local packet=S.loading and '1|1|0|0|0|0|0|1|1'or menu and '1|0|0|0|0|0|0|1|1'or '0|0|0|0|0|0|0|1|1'
    S.opacity=(S.loading or menu)and 1 or 0
    S.source=rawLoading and 'loading-state-cover'or S.awaitingDestination and 'waiting-destination-ready'or menu and 'menu-state-cover'or nil
    if packet~=S.coverPacket then
        call(api,'dispatch_custom_event','AC8_TransitionCover',packet)
        S.coverPacket=packet;

    end
end
uevr.sdk.callbacks.on_pre_engine_tick(update)
uevr.sdk.callbacks.on_post_engine_tick(function()update(nil,0)end)
uevr.sdk.callbacks.on_script_reset(function()S.stopped=true;clearCover()end)
end)(AC8Runtime.host('utility','general_vr_transitions.lua'),AC8Runtime.json,AC8Runtime.log)
end
assert(not registry.sections.loading_icon,'Duplicate loading icon')
registry.sections.loading_icon=function(R)
-- V70: center the captured shared ClientStatusIconParent loading indicator
-- widget. Preserve animation, content, scale, full-screen loading background.
do
local R=AC8Runtime;local host=R.host('loading','center loading icon')
local S={version=71,nextDiscover=0,rows={}};R.loadingIconCenter=S
local function call(o,k,...)
 if not o or(o~=uevr.api and not R.isLiveObject(o))then return end
 local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end
end
local function field(o,k)local ok,v=pcall(function()return o[k]end);if ok then return v end end
local function xy(v)
 local x,y=field(v,'X'),field(v,'Y');if type(x)=='number'and type(y)=='number'and x==x and y==y then return{X=x,Y=y}end
end
local function full(o)return call(o,'get_full_name')end
local function anchors(v)
 local a,b=xy(field(v,'Minimum')),xy(field(v,'Maximum'));if a and b then return{Minimum=a,Maximum=b}end
end
local function same(a,b)return a and b and math.abs(a.X-b.X)<.01 and math.abs(a.Y-b.Y)<.01 end
local function sameAnchors(a,b)return a and b and same(a.Minimum,b.Minimum)and same(a.Maximum,b.Maximum)end
local middle={X=.5,Y=.5};local centered={Minimum=middle,Maximum=middle};local zero={X=0,Y=0}
local slate=call(uevr.api,'find_uobject','Class /Script/UMG.SlateBlueprintLibrary');slate=call(slate,'get_class_default_object')
local function rect(o)
 local g=call(o,'GetPaintSpaceGeometry')or call(o,'GetCachedGeometry');local size=xy(call(slate,'GetLocalSize',g))
 if not size or size.X<=0 or size.Y<=0 then return end
 local a=xy(call(slate,'LocalToAbsolute',g,zero));local b=xy(call(slate,'LocalToAbsolute',g,size))
 if not a or not b then return end
 return{math.min(a.X,b.X),math.min(a.Y,b.Y),math.max(a.X,b.X),math.max(a.Y,b.Y)}
end
local function slotState(slot)
 return{anchors=anchors(call(slot,'GetAnchors')),alignment=xy(call(slot,'GetAlignment')),position=xy(call(slot,'GetPosition'))}
end
local function put(slot,a,alignment,position)
 call(slot,'SetAnchors',a);call(slot,'SetAlignment',alignment);call(slot,'SetPosition',position)
 local got=slotState(slot);return sameAnchors(got.anchors,a)and same(got.alignment,alignment)and same(got.position,position)
end
local function qualified(o)
 local slot=call(o,'get_property','Slot');if not(full(slot)or''):match('^CanvasPanelSlot ')then return end
 local old=slotState(slot);local a=old.anchors
 if not a or not old.alignment or not old.position then return end
 -- Fixed bottom-right anchors identify the authored corner container without
 -- moving the full-screen loading panel or guessing from its instance name.
 local known=full(o)or''
 if known:find('WBP_Loading_Root_000_C_',1,true)and known:find('.ClientStatusIconParent.',1,true)and known:match('%.HorizontalBox_61$')then return slot,old,rect(o)end
 if not same(a.Minimum,a.Maximum)or a.Minimum.X<.75 or a.Minimum.Y<.75 then return end
 local name=full(o)or''
 if name:find('WBP_Loading_Root_000_C_',1,true)and(name:match('%.ClientStatusIconParent$')or name:match('%.LoadingIcon$'))then return slot,old,rect(o)end
 local parent=call(o,'GetParent');local pr,wr=rect(parent),rect(o)
 if not pr or not wr then return end
 local pw,ph=pr[3]-pr[1],pr[4]-pr[2];local ww,wh=wr[3]-wr[1],wr[4]-wr[2]
 if pw<=0 or ph<=0 or ww<=0 or wh<=0 or ww>pw*.35 or wh>ph*.35 then return end
 local cx,cy=(wr[1]+wr[3])/2,(wr[2]+wr[4])/2
 if cx<pr[1]+pw*.65 or cy<pr[2]+ph*.65 then return end
 return slot,old,wr
end
local function iconContainer(owner)
 local queue={owner};local seen={};local index=1
 local tree=call(owner,'get_property','WidgetTree');local root=call(tree,'get_property','RootWidget')or call(owner,'GetRootWidget')
 if root then queue[#queue+1]=root end
 while index<=#queue and index<=64 do
  local w=queue[index];index=index+1
  if w and not seen[w]then
   seen[w]=true;local slot,old,bounds=qualified(w)
   if slot then return w,slot,old,bounds end
   local count=call(w,'GetChildrenCount')
   if type(count)=='number'then for i=0,math.min(count,32)-1 do if #queue<64 then queue[#queue+1]=call(w,'GetChildAt',i)end end end
   local content=call(w,'GetContent');if content and #queue<64 then queue[#queue+1]=content end
  end
 end
end

local function apply(row)
 if row.applied then return end
 local w,slot,old,bounds=iconContainer(row.owner)
 if not w then ;return end
 row.container=w;row.containerName=full(w);row.slot=slot;row.slotName=full(slot);row.original=old;row.beforeBounds=bounds
 if put(slot,centered,middle,zero)then row.applied=true;
 else put(slot,old.anchors,old.alignment,old.position);end
end
host.sdk.callbacks.on_post_engine_tick(function()
 -- Flight uses shared in-memory lifecycle state; do not query loading widgets.
 local c=R.context;local tr=rawget(_G,'AC8VRTransitions')
 local entering=tr and not tr.stopped and tr.hud==c.hudName and tr.loading==true
 if(c.flight or movie(AC8Runtime))and not(R.missionLoading and R.missionLoading.active)and not entering then
  S.suspended=true;return
 end
 if S.suspended then S.suspended=false;S.nextDiscover=R.tick end
 -- Class-specific tracked discovery every 60 ticks. Applied containers are cached;
 -- there is no steady-state geometry polling or all-widget scan.
 if S.epoch~=R.context.epoch then S.epoch=R.context.epoch;S.nextDiscover=R.tick end
 for o,row in pairs(S.rows)do
  if not R.isLiveObject(o)or(row.slot and not R.isLiveObject(row.slot))then S.rows[o]=nil
  elseif not row.applied and R.tick%4==0 then apply(row)end
 end
 if R.tick<S.nextDiscover then return end;S.nextDiscover=R.tick+60
 S.class=S.class or call(uevr.api,'find_uobject','Class /Script/Live.LiveLoadingClientStatusIconParentWidget')
 for i,owner in ipairs(call(S.class,'get_objects_matching',false)or{})do
  if i>16 then break end
  local name=full(owner)or''
  if name:find('/Engine/Transient.',1,true)then
   local row=S.rows[owner]or{owner=owner,name=name};S.rows[owner]=row;apply(row)
  end
 end
end)

host.sdk.callbacks.on_script_reset(function()
 for _,row in pairs(S.rows)do
  if row.applied and R.isLiveObject(row.slot)then
   local current=slotState(row.slot)
   if sameAnchors(current.anchors,centered)and same(current.alignment,middle)and same(current.position,zero)then
    put(row.slot,row.original.anchors,row.original.alignment,row.original.position)
   end
  end
 end
 S.rows={}
end)
end


end

if registry.start then registry.start()end


