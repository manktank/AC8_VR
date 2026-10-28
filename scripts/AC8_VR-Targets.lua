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
-- Same-file preference snapshot; no sibling module is needed. Reload by
-- restarting scripts/game. Keep each consumer's original validation semantics.
local settings={}
local function setting(json,name)
 if settings[name]==nil then
  local value=json.load_file(name)
  if value==nil and name=='AC8_VR-TargetSettings.json'then value=json.load_file('AC8_VR-WorkshopTargetControls.json')end -- Upgrade only.
  local copy={}
  if type(value)=='table'then for k,v in pairs(value)do copy[k]=v end end
  settings[name]=copy
 end
 return settings[name]
end
assert(not registry.sections['target_labels'], 'Duplicate AC8 VR section: target_labels')
registry.sections['target_labels'] = function(R)
do
-- V80: native scopes retain accepted text placement only. Box dimensions return
-- to the pre-V77 post-tick writer, leaving the introduction's initial size native.
(function(uevr,json)
local R=AC8Runtime;local S={version=80,packet=nil,ticks=0};R.worldTargetLayout=S
local function call(o,k,...)if not o then return end;local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end end
local function valid(v,lo,hi)return type(v)=='number'and v==v and v>=lo and v<=hi end
local function update()
 if not gameplay(AC8Runtime)then
  S.settingsAddress=nil
  local off='0|0|1|1|1|0|0|0|0'
  if S.packet~=off then call(uevr.api,'dispatch_custom_event','AC8_WorldTargetLayout',off);S.packet=off end
  return
 end
 local c=R.context;local level=gameplay(AC8Runtime)and call(c.hud,'get_outer');local address=R.isLiveObject(level)and call(level,'get_address')or 0
 if not address then address=0 end
 if S.settingsAddress==address then return end
 S.settingsAddress=address
 local t=setting(json,'AC8_VR-TargetSettings.json')or{}
 local w=setting(json,'AC8_VR-WorldTargetControls.json')or{}
 local box,label,font,spacing=t.boxScale,t.labelScale,t.fontScale or 1,t.lineSpacing or 0
 if not valid(box,.1,2)or not valid(label,.1,2)or not valid(font,.1,2)or not valid(spacing,-200,200)then box,label,font,spacing=1,1,1,0 end
 local raise=valid(w.labelRaise,-200,200)and w.labelRaise or 0
 local leftDrop=valid(w.leftLabelDrop,-200,200)and w.leftLabelDrop or 0
 local on=address~=0 and t.enabled==true and w.enabled~=false
 local packet=string.format('%d|%d|%.9g|%.9g|%.9g|%.9g|%d|%.9g|%.9g',address,on and 1 or 0,box,label,font,spacing,w.useFlatLabelOffsets~=false and 1 or 0,raise,leftDrop)
 if packet~=S.packet then call(uevr.api,'dispatch_custom_event','AC8_WorldTargetLayout',packet);S.packet=packet end
 S.controls={enabled=on,boxScale=box,labelScale=label,fontScale=font,lineSpacing=spacing,useFlatLabelOffsets=w.useFlatLabelOffsets~=false,labelRaise=raise,leftLabelDrop=leftDrop}
end
uevr.sdk.callbacks.on_pre_engine_tick(function()S.ticks=S.ticks+1;update()end)
pcall(function()uevr.sdk.callbacks.on_script_reset(function()call(uevr.api,'dispatch_custom_event','AC8_WorldTargetLayout','0|0|1|1|1|0|0|0')end)end)
end)(AC8Runtime.host('utility','native world target layout'),AC8Runtime.json)
end


end
assert(not registry.sections['target_animation'], 'Duplicate AC8 VR section: target_animation')
registry.sections['target_animation'] = function(R)
-- Trial: preserve the accepted POST-engine application phase, but apply the
-- configured dimensions only on first discovery or an explicit scale change.
-- No dimension reads or corrective writes between discovery/settings changes.
(function(uevr,json)
local R=AC8Runtime
local S={version=103,mode='size once per discovered target',ticks=0,epoch=-1,rows={}}
R.worldBoxDimensionRollback=S
local fields={'ContainerBoxImageSize','SelectTargetAddContainerBoxSize'}
local function call(o,k,...)
 if not o or(o~=uevr.api and not R.isLiveObject(o))then return end
 local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end
end
local function read(o,key)return call(o,'get_property',key)end
local function restore()
 for _,row in pairs(S.rows)do
  if call(row.actor,'get_full_name')==row.name then
   for _,key in ipairs(fields)do
    if row.last[key]~=nil and read(row.actor,key)==row.last[key]then call(row.actor,'set_property',key,row.original[key])end
   end
  end
 end
 S.rows={};S.lastComponents=nil;S.lastCount=nil;S.lastFactor=nil
end
uevr.sdk.callbacks.on_post_engine_tick(function()
 S.ticks=S.ticks+1;local ctx=R.context
 if S.epoch~=ctx.epoch then restore();S.epoch=ctx.epoch;S.coldEpoch=nil end
 if not gameplay(AC8Runtime)then return end
 local control=setting(json,'AC8_VR-TargetSettings.json')or{}
 local factor=control.enabled==true and tonumber(control.boxScale)or 1
 if not factor or factor~=factor or factor<.1 or factor>2 then return end
 if factor==1 and not next(S.rows)then return end
 local pins=rawget(_G,'AC8TargetPinsV56')
 local components=pins and pins.hud==ctx.hudName and pins.components
 if not components then
  if S.coldEpoch~=ctx.epoch then
   local cls=call(uevr.api,'find_uobject','Class /Script/Engine.MaterialBillboardComponent')
   S.cold=call(cls,'get_objects_matching',false)or{};S.coldEpoch=ctx.epoch
  end
  components=S.cold or{}
 end
 local count=#components
 if S.lastComponents==components and S.lastCount==count and S.lastFactor==factor then
  return
 end
 
 local level=(ctx.hudName or''):match('^LiveMainHUDParent4K_C (.+%.PersistentLevel)%.');if not level then return end
 local seen={}
 for _,component in ipairs(components)do
  local name=call(component,'get_full_name')or''
  if name:find(level..'.ChronicleTargetContainerActorBP_C_',1,true)and name:match('%.ContainerBox$')then
   local actor=call(component,'GetOwner');local n=call(actor,'get_full_name')
   if n and not seen[n]then
    seen[n]=true;local row=S.rows[n]
    if row and row.actor~=actor then row=nil end
    if not row then
     local original={};for _,key in ipairs(fields)do original[key]=read(actor,key)end
     local a,b=original[fields[1]],original[fields[2]]
     if type(a)=='number'and type(b)=='number'and a==a and b==b then
      row={actor=actor,name=n,original=original,last={}};S.rows[n]=row
     end
    end
    -- Rediscovery is NOT authorization to reassert the same scale. If the
    -- game changed an existing target's size, leave that change untouched.
    if row and row.factor~=factor then
     for _,key in ipairs(fields)do
      local wanted=row.original[key]*factor
      if read(actor,key)~=wanted then call(actor,'set_property',key,wanted);end
      row.last[key]=wanted
     end
     row.factor=factor
    end
   end
  end
 end
 for n in pairs(S.rows)do if not seen[n]then S.rows[n]=nil end end
 S.lastComponents=components;S.lastCount=count;S.lastFactor=factor
end)
uevr.sdk.callbacks.on_script_reset(restore)
end)(AC8Runtime.host('utility','world box animation rollback'),AC8Runtime.json)

end

assert(not registry.sections['target_pins'], 'Duplicate AC8 VR section: target_pins')
registry.sections['target_pins'] = function(R)
-- Keep the original discovery cadence, live per-axis offsets and viewport phase.
(function(uevr,json,print)
local api=uevr.api
local previous=rawget(_G,'AC8TargetPinsV56');if previous and previous.restore then previous.restore()end
local S={version=104,ticks=0,rows={},pinActors={}}
rawset(_G,'AC8TargetPinsV56',S)
local worldKeys={'ClockTargetPinOffset','AntiClockTargetPinOffset'}
local locatorKeys={'TargetPinSize'}
local function call(o,k,...)if not o then return end;local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end end
local function live(o)return AC8Runtime.isLiveObject(o)end
local function full(o)if live(o)then return call(o,'get_full_name')end end
local function prop(o,k)return call(o,'get_property',k)end
local function xy(v)
 local ok,x,y=pcall(function()return v.X,v.Y end)
 if ok and type(x)=='number'and type(y)=='number'and x==x and y==y and math.abs(x)<10000 and math.abs(y)<10000 then return {X=x,Y=y}end
end
local mathLib=call(api,'find_uobject','KismetMathLibrary /Script/Engine.Default__KismetMathLibrary')
local function equal(a,b)return a and b and math.abs(a.X-b.X)<.0001 and math.abs(a.Y-b.Y)<.0001 end
local function put(o,key,v)
 local made=call(mathLib,'MakeVector2D',v.X,v.Y)
 if made then call(o,'set_property',key,made)end
 if not equal(xy(prop(o,key)),v)then pcall(function()local p=o:get_property(key);p.X=v.X;p.Y=v.Y end)end
 return equal(xy(prop(o,key)),v)
end
local function restore(row)
 if full(row.object)~=row.name then return end
 for key,v in pairs(row.original)do if equal(xy(prop(row.object,key)),row.last[key])then put(row.object,key,v)end end
end
-- Refueling now uses the native actor's shared layout dimensions. No independent
-- font-size or text-component-position adapter is installed here.
S.restore=function()
 call(api,'dispatch_custom_event','AC8_WorldPipScale','1');S.stopped=true
 for _,row in pairs(S.rows)do restore(row)end
 S.rows={};S.pinActors={};S.components=nil
end
local function apply(o,keys,kind,factor,seen,expectedName)
 local name=full(o);if not name or(expectedName and name~=expectedName)or seen[name]then return end
 seen[name]=true
 local row=S.rows[name]
 if row and row.object~=o then restore(row);S.rows[name]=nil;row=nil end
 if not row then
  local original={};for _,key in ipairs(keys)do original[key]=xy(prop(o,key));if not original[key]then return end end
  row={name=name,object=o,kind=kind,original=original,last={}};S.rows[name]=row
 end
 for _,key in ipairs(keys)do
  local current=xy(prop(o,key));local base=row.original[key];local last=row.last[key]
  if current and last then
   if math.abs(current.X-last.X)>.0001 then base.X=current.X end
   if math.abs(current.Y-last.Y)>.0001 then base.Y=current.Y end
  end
  local wanted={X=base.X*factor,Y=base.Y*factor}
  if not equal(current,wanted)then
   if put(o,key,wanted)then else end
  end
  row.last[key]=wanted
 end
end
local function discover(level,factor)
 -- Preserve this complete list for the accepted size-once module's discovery.
 S.components=call(call(api,'find_uobject','Class /Script/Engine.MaterialBillboardComponent'),'get_objects_matching',false)or{}
 local actors,byObject={},{}
 for _,component in ipairs(S.components)do
  
  local name=full(component)or''
  if name:find(level..'.ChronicleTargetContainerActorBP_C_',1,true)and(name:match('%.ClockTargetPinBillboardImage$')or name:match('%.AntiClockTargetPinBillboardImage$'))then
   local actor=call(component,'GetOwner');local actorName=full(actor)
   if actorName then
    local entry=byObject[actor]
    if not entry then entry={object=actor,name=actorName,components={}};byObject[actor]=entry;actors[#actors+1]=entry end
    entry.components[#entry.components+1]=component
   end

  end
 end
 S.pinActors=actors;
end
uevr.sdk.callbacks.on_pre_engine_tick(function()
 if S.stopped or not gameplay(AC8Runtime)then return end
 local controls=setting(json,'AC8_VR-TargetSettings.json')
 local factor=type(controls)=='table'and controls.enabled and tonumber(controls.boxScale)or 1
 if not factor or factor~=factor or factor<.1 or factor>2 then return end
 if S.nativeScale~=factor then call(api,'dispatch_custom_event','AC8_WorldPipScale',tostring(factor));S.nativeScale=factor end
end)
uevr.sdk.callbacks.on_pre_viewport_client_draw(function()
 if S.stopped or movie(AC8Runtime)or(AC8Runtime.context.flight and not gameplay(AC8Runtime))then return end;S.ticks=S.ticks+1
 local controls=setting(json,'AC8_VR-TargetSettings.json')
 local factor=type(controls)=='table'and controls.enabled and tonumber(controls.boxScale)or 1
 if not factor or factor~=factor or factor<.1 or factor>2 then return end
 local hud=call(call(api,'get_player_controller',0),'GetHUD');local hn=full(hud)or''
 local level=hn:match('^LiveMainHUDParent4K_C (.+%.PersistentLevel)%.');local seen={}
 if level then
  if S.ticks%15==1 or S.hud~=hn or S.hudObject~=hud then discover(level,factor)end
  S.hud=hn;S.hudObject=hud
  for _,entry in ipairs(S.pinActors)do
   local componentAlive=false
   for _,component in ipairs(entry.components)do if live(component)then componentAlive=true;break end end
   if componentAlive then apply(entry.object,worldKeys,'world-actor-offsets',factor,seen,entry.name)end
  end
  -- Keep this live: locator widgets can appear between world discovery scans.
  local canvas=prop(hud,'DynamicCanvas')
  for i=0,math.min(call(canvas,'GetChildrenCount')or 0,64)-1 do
   local w=call(canvas,'GetChildAt',i);local n=full(w)or''
   if n:match('^WBP_HUD_DynamicWidget_Parts_Locator_000_C ')and n:match('%.WBP_LocatorWidget$')then apply(w,locatorKeys,'HUD-locator',factor,seen)end
  end
 else
  -- Force fresh discovery on re-entry, even if the HUD name gets reused.
  S.hud=nil;S.hudObject=nil;S.pinActors={};S.components=nil
 end
 for name,row in pairs(S.rows)do if not seen[name]then restore(row);S.rows[name]=nil end end

end)
pcall(function()uevr.sdk.callbacks.on_script_reset(S.restore)end)
end)(AC8Runtime.host('utility','AC8_VR-TargetPinsV56.lua'),AC8Runtime.json,AC8Runtime.log)

end

assert(not registry.sections['target_materials'], 'Duplicate AC8 VR section: target_materials')
registry.sections['target_materials'] = function(R)
-- Trial: game owns target colors. No saturation/brightness reads, setters or polling.
R.worldTargetSaturation={version=102,enabled=false,gameOwnedColors=true,trial=true,saved={}}
end


assert(not registry.sections.target_presentation,'Duplicate target presentation')
registry.sections.target_presentation=function(R)
 local last
 local function update()
  local active=gameplay(R)
  if active~=last then uevr.api:dispatch_custom_event('AC8_TargetsPresentation',active and'on'or'off');last=active end
 end
 local host=R.host('admission','target presentation admission')
 host.sdk.callbacks.on_pre_engine_tick(update)
 host.sdk.callbacks.on_post_engine_tick(update)
end

if registry.start then registry.start()end


