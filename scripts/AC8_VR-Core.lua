-- AC8 modular release.
-- Core: see source_code/README.md for ownership and dependencies.
-- AC8 VR: register sections; Core starts them in explicit dependency order.
-- Core stays loaded. Optional files may be absent; selection freezes on the first engine tick.
local registry = rawget(_G, 'AC8VRModules')
if not registry then
 registry = {sections={}, state='waiting', completed={}}
 rawset(_G, 'AC8VRModules', registry)
end
-- Shared pause/tutorial support, owned by Core.
local function ensureTutorialState(R)
 if R.tutorialState then return R.tutorialState end
 local T={version=1,visible=false,widgets={},roots={},contents={}}
 R.tutorialState=T
 local function call(o,k,...)
  if not o or(o~=uevr.api and not R.isLiveObject(o))then return end
  local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end
 end
 local function full(o)return call(o,'get_full_name')end
 local function prop(o,k)return call(o,'get_property',k)end
 local S=T
-- Only roots need a class query. Tutorial contents are owned by those roots;
-- their class need not be loaded before an ordinary pause dialog appears.
local tutorialClasses={}
local rootPaths={
 'WidgetBlueprintGeneratedClass /Game/NUI/Common/WBP_Menu_Common_Dialog_000.WBP_Menu_Common_Dialog_000_C',
 'Class /Script/Live.LiveNUICommonDialogWidget',
}
local function dialogRoots()
 for index,path in ipairs(rootPaths)do
  local cls=tutorialClasses[path];local identity=full(cls)
  if identity~=path then cls=call(uevr.api,'find_uobject',path);tutorialClasses[path]=cls;identity=full(cls)end
  if identity==path then
   local objects=call(cls,'get_objects_matching',false)
   if type(objects)=='table'then
    
    return objects
   end
  end
 end
 
 return {}
end
local function dialogContents(root,rootName)
 local tree=prop(root,'WidgetTree');local first=prop(tree,'RootWidget')
 local path=rootName:match('^%S+ (.+)$');local out={}
 if not first or not path then return out end
 local queue,seen={first},{};local index=1
 while index<=#queue and index<=4096 do
  local w=queue[index];index=index+1
  if w and not seen[w]then
   seen[w]=true;local n=full(w)or'';local p=n:match('^%S+ (.+)$')
   -- Never collect contents from another live dialog/mission.
   if p and p:sub(1,#path+1)==path..'.'then
    if n:match('^WBP_Menu_Common_Dialog_Parts_Contents_CP_002_C ')then out[#out+1]=w end
    local count=call(w,'GetChildrenCount')
    if type(count)=='number'and count>=0 and count%1==0 then
     for i=0,math.min(count,512)-1 do
      local child=call(w,'GetChildAt',i)
      if child and #queue<4096 then queue[#queue+1]=child end
     end
     end
    local nested=prop(prop(w,'WidgetTree'),'RootWidget')
    if nested and #queue<4096 then queue[#queue+1]=nested end
   end
  end
 end
 return out
end

local function discoverTutorials()
 local rows={}
 for i,root in ipairs(dialogRoots())do
  if i>4096 then break end
  local n=full(root)or''
  if n:match('^WBP_Menu_Common_Dialog_000_C /Engine/Transient%.')and not n:find('Default__',1,true)then
   rows[#rows+1]=root
   -- Include hidden contents too: showing an existing dialog between refreshes
   -- must still apply immediately using the same cached instances.
   for _,w in ipairs(dialogContents(root,n))do rows[#rows+1]=w end
  end
 end
 return rows
end


 local function active(w)
  if not call(w,'GetParent')then return false end
  for _=1,12 do
   local v=call(w,'GetVisibility')
   if v==1 or v==2 or call(w,'GetRenderOpacity')==0 then return false end
   local parent=call(w,'GetParent');if not parent then return true end
   local selected=call(parent,'GetActiveWidget')
   if selected and full(selected)~=full(w)then return false end
   w=parent
  end
  return false
 end
 function T.get(pc,hn,paused,tick)
  tick=R.tick or tick or 0
  if paused~=true or not(hn or''):match('^LiveMainHUDParent4K_C ')then
   if T.paused or T.visible then T.visible=false;T.roots={};T.contents={};T.paused=false;T.stamp=nil end
   return T
  end
  if T.stamp==tick and T.hud==hn then return T end
  local edge=not T.paused or T.hud~=hn
  T.stamp=tick;T.hud=hn;T.paused=true
  if edge or not T.candidates or tick%30==0 then
   T.candidates={}
   for _,w in ipairs(discoverTutorials())do
    local n=full(w)or''
    if n:find('/Engine/Transient.',1,true)and(n:match('^WBP_Menu_Common_Dialog_000_C ')or n:match('^WBP_Menu_Common_Dialog_Parts_Contents_CP_002_C '))then
     T.candidates[#T.candidates+1]={name=n,object=w}
    end
   end
  end
  local widgets,roots,contents={},{},{}
  for _,c in ipairs(T.candidates or{})do
   local w=c.object
   if full(w)~=c.name then w=call(uevr.api,'find_uobject',c.name);if full(w)~=c.name then w=nil end;c.object=w end
   if w then
    widgets[#widgets+1]=w;local n=c.name
    if not n:find('Default__',1,true)then
     if n:match('^WBP_Menu_Common_Dialog_000_C ')and call(w,'GetVisibility')==0 then roots[#roots+1]={object=w,name=n,path=n:match('^%S+ (.+)$')}end
     if n:match('^WBP_Menu_Common_Dialog_Parts_Contents_CP_002_C ')and active(w)then contents[#contents+1]={object=w,name=n}end
    end
   end
  end
  T.widgets=widgets;T.roots=roots;T.contents=contents;T.visible=false
  for _,root in ipairs(roots)do
   for _,c in ipairs(contents)do if c.name:find(root.path..'.',1,true)then T.visible=true;break end end
   if T.visible then break end
  end
  return T
 end
 return T
end

-- End Core tutorial-state helper.
-- Shared HUD defaults are independent of optional aircraft presets.
local function newHUDDefaultsStore(files)
 local D={file='AC8_VR-HUDDefaults.json',backup='AC8_VR-HUDDefaults.backup.json'}
 local function copy(v)if type(v)~='table'then return v end;local t={};for k,x in pairs(v)do t[k]=copy(x)end;return t end
 local function equal(a,b)
  if type(a)~=type(b)then return false end;if type(a)~='table'then return a==b end
  for k,x in pairs(a)do if not equal(x,b[k])then return false end end
  for k in pairs(b)do if a[k]==nil then return false end end;return true
 end
 local function param(p)
  return type(p)=='table'and type(p.enabled)=='boolean'and type(p.x)=='number'and p.x==p.x and math.abs(p.x)<=4000 and
   type(p.y)=='number'and p.y==p.y and math.abs(p.y)<=4000 and type(p.scale)=='number'and p.scale>=.1 and p.scale<=3
 end
 local function layout(v)
  if type(v)~='table'or type(v.groups)~='table'or(v.components~=nil and type(v.components)~='table')then return end
  for _,k in ipairs({'ladder','radar','info'})do if not param(v.groups[k])then return end end
  local n=0;for k,p in pairs(v.components or{})do n=n+1;if n>24 or type(k)~='string'or not param(p)then return end end
  if v.canvas then local c=v.canvas
   if not param(c)or type(c.extraWidth)~='number'or c.extraWidth~=c.extraWidth or c.extraWidth<0 or c.extraWidth>4000 or
    type(c.extraHeight)~='number'or c.extraHeight~=c.extraHeight or c.extraHeight<0 or c.extraHeight>4000 or type(c.inheritClip)~='boolean'then return end
  end
  local out=copy(v);out.components=out.components or{};out.simpleGroups=out.simpleGroups or{};out.simpleGroups.navigation=nil
  for k,p in pairs(out.simpleGroups)do
   if type(k)~='string'or type(p)~='table'or type(p.x)~='number'or p.x~=p.x or math.abs(p.x)>4000 or type(p.y)~='number'or p.y~=p.y or math.abs(p.y)>4000 then return end
  end
  return out
 end
 local function decode(v)
  if type(v)~='table'or v.version~=1 then return end
  local a,b=layout(v.default),layout(v.shared);if a and b then return{version=1,default=a,shared=b}end
 end
 function D:init()
  if self.db then return true end
  local ok,v=pcall(files.load_file,self.file);local valid=ok and decode(v)
  if not valid then local good,b=pcall(files.load_file,self.backup);valid=good and decode(b)end
  if valid then self.db=valid;return true end
  local good,old=pcall(files.load_file,'AC8_VR-HUDLayouts.json')
  if good and type(old)=='table'then
   local a,b=layout(old.default),layout(old.allPlanes or old.default)
   if a and b then self.db={version=1,default=a,shared=b};return true end
  end
  local p={groups={},components={},simpleGroups={}}
  for _,k in ipairs({'ladder','radar','info'})do p.groups[k]={enabled=false,x=0,y=0,scale=1}end
  self.db={version=1,default=copy(p),shared=copy(p)}
  if not ok or v~=nil then self.invalid=true;self.message='Invalid shared HUD defaults; existing files preserved.'end
  return true
 end
 function D:saveShared(value,global)
  self:init();local next=layout(value)
  if self.invalid or not next then return false,self.message or'Invalid shared HUD layout.'end
  local db={version=1,default=global and copy(next)or copy(self.db.default),shared=next}
  local good,result=pcall(files.dump_file,self.backup,self.db,2);local readOK,b=pcall(files.load_file,self.backup)
  if not good or result==false or not readOK or not equal(decode(b),self.db)then return false,'Shared HUD backup failed verification.'end
  good,result=pcall(files.dump_file,self.file,db,2);readOK,b=pcall(files.load_file,self.file)
  if not good or result==false or not readOK or not equal(decode(b),db)then pcall(files.dump_file,self.file,self.db,2);return false,'Shared HUD save failed; previous layout retained.'end
  self.db=db;return true,global and'Global HUD default saved; aircraft presets unchanged.'or'Saved shared HUD layout.'
 end
 function D:saveGlobal(value)return self:saveShared(value,true)end
 function D:view()
  local P={message='AC8_VR-AircraftPresets.lua is absent; general HUD controls remain available.'}
  function P:init()D:init();self.db={version=1,perPlaneEnabled=false,useDefaultForAllPlanes=true,default=copy(D.db.default),allPlanes=copy(D.db.shared),planes={}};return true end
  function P:selected()return copy(D.db.shared),'all planes'end
  function P:saveAll(v)local ok,msg=D:saveShared(v);if ok then self:init();self.force=true end;return ok,msg end
  function P:saveGlobalHUD(v)local ok,msg=D:saveGlobal(v);if ok then self:init();self.force=true end;return ok,msg end
  function P:count()return 0 end
  function P:savePlane()return false,self.message end
  P.setPerPlaneEnabled=P.savePlane;P.setShared=P.savePlane;P.saveUISizePreferences=P.savePlane
  P.copy=copy;P.layout=layout;P.plane=function()return false end
  P:init();return P
 end
 D.copy=copy;D.layout=layout;return D
end
-- End shared HUD defaults helper.

assert(not registry.sections['core'], 'Duplicate AC8 VR section: core')
registry.sections['core'] = function(R)
-- AC8 utility services and inherited Flat2VR adaptations.
-- Inherited code is attributed in the sections below; original history is backed up.
local real=uevr;local realJSON=json
local R={version=105,tick=0,domains={},connected={},context={epoch=0},cache={},errors={},inheritedSetup={},layoutEnabled=false}
_G.AC8Runtime=R
R.ensureTutorialState=function()return ensureTutorialState(R)end
R.newHUDDefaultsStore=newHUDDefaultsStore
local order={'presentation','admission','layout','camera','utility','loading','intro','development'}
local function invoke(o,k,...)local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end end
-- UObjectHook tracks destruction; membership checks do not walk reflected fields.
local objectHook=rawget(_G,'UEVR_UObjectHook')or(real.types and real.types.UObjectHook)
local exists=objectHook and objectHook.exists
R.objectLifetimeCheckAvailable=type(exists)=='function'
if not R.objectLifetimeCheckAvailable then R.errors.lifetime='UObjectHook.exists unavailable; unsafe UObject access skipped' end
function R.isLiveObject(o)
 if not o or type(exists)~='function'then return false end
 local ok,alive=pcall(exists,o);return ok and alive==true
end
-- UObject calls require lifetime membership. SDK tables and returned geometry
-- structs use their own helpers; they must not be sent through this guard.
function R.objectCall(o,k,...)
 if not R.isLiveObject(o)then return nil end
 return invoke(o,k,...)
end
-- Shared discovery within one engine tick. Walk only the requested widget's
-- painted hierarchy, including nested UserWidget trees. Callers retain their
-- existing visibility, clipping and owner checks and their discovery cadence.
-- An unavailable root or incomplete walk returns nil for the legacy fallback.
local widgetWalk={}
function R.ownedWidgets(root,fresh)
 if widgetWalk.tick~=R.tick or widgetWalk.epoch~=R.context.epoch then
  widgetWalk={tick=R.tick,epoch=R.context.epoch,roots={}}
 end
 local name=R.objectCall(root,'get_full_name')
 if not name then return nil end
 local cached=widgetWalk.roots[name]
 if not fresh and cached and cached.root==root then
  local alive=true
  for _,w in ipairs(cached.rows)do if not R.isLiveObject(w)then alive=false;break end end
  if alive then return cached.rows,cached.names end
 end
 local tree=R.objectCall(root,'get_property','WidgetTree')
 local first=R.objectCall(tree,'get_property','RootWidget')
 if not R.isLiveObject(first)then return nil end
 local rows,queue,seen,names={},{root},{},{}
 local index=1
 while index<=#queue do
  if index>8192 then return nil end
  local w=queue[index];index=index+1
  local n=R.objectCall(w,'get_full_name')
  if not n then return nil end
  if not seen[n]then
   seen[n]=true;rows[#rows+1]=w;names[w]=n
   local count=R.objectCall(w,'GetChildrenCount')
   if count~=nil then
    if type(count)~='number'or count<0 or count>4096 or count%1~=0 then return nil end
    for i=0,count-1 do
     local child=R.objectCall(w,'GetChildAt',i)
     if not R.isLiveObject(child)then return nil end
     queue[#queue+1]=child
    end
   end
   local wt=R.objectCall(w,'get_property','WidgetTree')
   if wt then
    local child=R.objectCall(wt,'get_property','RootWidget')
    if not R.isLiveObject(child)then return nil end
    queue[#queue+1]=child
   end
  end
 end
 widgetWalk.roots[name]={root=root,rows=rows,names=names}
 return rows,names
end
-- Facts are refreshed lazily once per callback phase, only if a selected feature asks.
local current={epoch=0};local contextStamp=-1;local factStamp=-1;local facts={};local phase=0
local function refresh()
 if contextStamp==phase then return end;contextStamp=phase
 local pc=invoke(real.api,'get_player_controller',0);if not R.isLiveObject(pc)then pc=nil end
 local hud=R.objectCall(pc,'GetHUD');if not R.isLiveObject(hud)then hud=nil end
 local root=R.objectCall(hud,'get_property','HudWidget');if not R.isLiveObject(root)then root=nil end
 local hn=R.objectCall(hud,'get_full_name')or'';local rn=R.objectCall(root,'get_full_name')or''
 local identity=hn..'|'..rn
 if identity~=current.identity then current.epoch=current.epoch+1;current.changedAt=R.tick;current.identity=identity end
 current.pc=pc;current.hud=hud;current.root=root;current.hudName=hn;current.rootName=rn
 current.flight=hn:match('^LiveMainHUDParent4K_C ')~=nil
end
R.context=setmetatable({},{__index=function(_,key)refresh();return current[key]end})
local function fact(key,reader)
 if factStamp~=phase then factStamp=phase;facts={}end
 if not facts[key]then facts[key]={reader()}end
 return table.unpack(facts[key])
end
function R.viewTargetName()
 return fact('viewTargetName',function()return R.objectCall(R.objectCall(R.context.pc,'GetViewTarget'),'get_full_name')or''end)
end
local statics
function R.gameStatics()
 if not R.isLiveObject(statics)then statics=invoke(real.api,'find_uobject','GameplayStatics /Script/Engine.Default__GameplayStatics')end
 return statics
end
function R.paused()return fact('paused',function()return R.objectCall(R.gameStatics(),'IsGamePaused',R.context.pc)end)end
function R.matchState()
 return fact('matchState',function()
  local state=R.objectCall(R.gameStatics(),'GetGameState',R.context.pc)
  local name=R.objectCall(state,'get_full_name')or''
  local level=(R.objectCall(R.context.pc,'get_full_name')or''):match('%s(.+%.PersistentLevel)%.')
  if not level or not name:match('^LiveGameState ')or not name:find(' '..level..'.',1,true)then return nil end
  local v=R.objectCall(state,'get_property','MatchState')
  if type(v)=='string'then return v end
  if v~=nil then local ok,text=pcall(function()return v:to_string()end);if ok and type(text)=='string'then return text end end
 end)
end
R.settingOwners={}
function R.setOwnership(owner,channel,active)
 local rows=R.settingOwners[channel]or{};R.settingOwners[channel]=rows;rows[owner]=active==true or nil
end
function R.settingsBusy(channel,owner)
 for key in pairs(R.settingOwners[channel]or{})do if key~=owner then return true end end;return false
end
-- Generic one-time ownership recovery when a removed feature left a settings journal.
-- No UObject calls, classifiers or steady-state callback are involved.
function R.recoverSettings(file,allowed)
 local ok,journal=pcall(realJSON.load_file,file)
 if not ok or type(journal)~='table'or type(journal.values)~='table'or next(journal.values)==nil then return true end
 local values,before={},{}
 for key,value in pairs(journal.values)do
  if not allowed[key]or(type(value)~='string'and type(value)~='number')then R.errors[file]='Invalid recovery settings';return false end
  values[key]=tostring(value)
 end
 local function get(key)local good,v=pcall(function()return real.params.vr:get_mod_value(key)end);if good and v~=nil then return tostring(v)end end
 local function same(a,b)return a==b or(tonumber(a)and tonumber(b)and tonumber(a)==tonumber(b))end
 for key in pairs(values)do before[key]=get(key);if before[key]==nil then R.errors[file]='Settings unavailable for removed-feature recovery';return false end end
 for key,value in pairs(values)do
  local good=pcall(function()real.params.vr.set_mod_value(key,value)end)
  if not good or not same(get(key),value)then
   for restore,v in pairs(before)do pcall(function()real.params.vr.set_mod_value(restore,v)end)end
   R.errors[file]='Removed-feature recovery write failed; previous values restored';return false
  end
 end
 local saved=pcall(function()real.params.vr.save_config()end)
 if not saved then R.errors[file]='Recovered in memory; save unavailable, journal retained';return false end
 local cleared=pcall(realJSON.dump_file,file,{version=1,values={}},2)
 if not cleared then R.errors[file]='Recovered settings; journal could not be cleared';return false end
 return true
end
function R.contextTick(advance)
 if advance~=false then R.tick=R.tick+1 end
 phase=phase+1
end
function R.connect(key)
 if R.connected[key]then return end;R.connected[key]=true
 real.sdk.callbacks[key](function(...)
  if key=='on_pre_engine_tick'then
   if registry.state=='waiting'then registry.start(true)end
   R.contextTick()
  elseif key=='on_post_engine_tick'or key=='on_pre_viewport_client_draw'then R.contextTick(false)end
  for _,domain in ipairs(order)do
   local rows=(domain~='layout'or R.layoutEnabled)and R.domains[domain]and R.domains[domain][key]
   for _,row in ipairs(rows or{})do local ok,err=pcall(row.fn,...);if not ok then R.errors[row.name]=tostring(err)end end
  end
 end)
end
function R.host(domain,name)
 R.domains[domain]=R.domains[domain]or {}
 local callbacks=setmetatable({},{__index=function(_,key)return function(fn)
  local d=R.domains[domain];d[key]=d[key]or {};d[key][#d[key]+1]={fn=fn,name=name..':'..key};R.connect(key)
 end end})
 return setmetatable({sdk=setmetatable({callbacks=callbacks},{__index=real.sdk})},{__index=real})
end
function R.read(name)
 local c=R.cache[name];if c and R.tick-c.tick<30 then return c.value end
 local ok,value=pcall(realJSON.load_file,name);R.cache[name]={tick=R.tick,value=ok and value or nil};return ok and value or nil
end
local journals={['AC8_VR-CockpitScaleRestore.json']=true,['AC8_VR-CinematicUIRestore.json']=true,['AC8_VR-HangarRestoreV48.json']=true,['AC8_VR-Ladder.json']=true,['AC8_VR-HUDLayoutCache.json']=true,['AC8_VR-WingmanSelectorBaseline.json']=true,['AC8_VR-WingmanStatusBaseline.json']=true}
R.json={load_file=R.read,dump_file=function(name,value,indent)
 if journals[name]then R.cache[name]=nil;return realJSON.dump_file(name,value,indent)end
end}
function R.log()end
R.connect('on_pre_engine_tick')


end


-- Core pause and tutorial features.
assert(not registry.sections['tutorial_fill'], 'Duplicate AC8 VR section: tutorial_fill')
registry.sections['tutorial_fill'] = function(R)
local tutorialState=ensureTutorialState(R)
do
-- Native tutorial card: the game owns Img_Base, its bounds, tint and fades.
(function(uevr,json,print)
local existing=rawget(_G,'AC8_TutorialFill')
if existing then assert(existing.stop(),'Previous tutorial originals could not be restored')end
local S={version=11,stopped=false,ticks=0,records={}}
rawset(_G,'AC8_TutorialFill',S)
local function call(o,k,...)
 if not o or(o~=uevr.api and not R.isLiveObject(o))then return nil end
 local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end
end
local function full(o)return call(o,'get_full_name')end
local function prop(o,k)return call(o,'get_property',k)end
local function resolve(name,object)
 if object and full(object)==name then return object end
 local found=call(uevr.api,'find_uobject',name);if full(found)==name then return found end
end
local function redraw(r)
 local root=resolve(r.rootName,r.rootObject);if not root then return end
 call(root,'InvalidateLayoutAndVolatility')
 local p=root
 for _=1,12 do
  if(full(p)or''):match('^RetainerBox ')then call(p,'RequestRender')end
  p=call(p,'GetParent');if not p then break end
 end
end
local function restore(name)
 local r=S.records[name];if not r then return true end
 local blur=resolve(r.blurName,r.blurObject)
 if blur and call(blur,'GetVisibility')==1 and r.originalVisibility~=1 then
  call(blur,'SetVisibility',r.originalVisibility)
  if call(blur,'GetVisibility')~=r.originalVisibility then return false end
  redraw(r)
 end
 S.records[name]=nil;return true
end
local function restoreAll()
 local ok,names=true,{};for n in pairs(S.records)do names[#names+1]=n end
 for _,n in ipairs(names)do if not restore(n)then ok=false end end;return ok
end
function S.stop()local ok=restoreAll();if ok then S.stopped=true end;return ok end
local function hide(r)
 local blur=resolve(r.blurName,r.blurObject);if not blur then return false end
 if call(blur,'GetVisibility')~=1 then
  call(blur,'SetVisibility',1)
  if call(blur,'GetVisibility')~=1 then return false end
  redraw(r)
 end
 return true
end
local statics
local function paused(pc)
 if type(R.paused)=='function'then return R.paused()end
 if not statics then statics=call(uevr.api,'find_uobject','GameplayStatics /Script/Engine.Default__GameplayStatics')end
 return call(statics,'IsGamePaused',pc)
end
local function late()
 if S.stopped then return end
 for _,r in pairs(S.records)do hide(r)end
end
pcall(function()uevr.sdk.callbacks.on_pre_engine_tick(function()
 if S.stopped or not next(S.records)then return end
 local pc=call(uevr.api,'get_player_controller',0)
 if paused(pc)~=true then restoreAll();return end
 late()
end)end)
pcall(function()uevr.sdk.callbacks.on_pre_viewport_client_draw(late)end)

uevr.sdk.callbacks.on_post_engine_tick(function()
 if S.stopped then return end
 S.ticks=S.ticks+1
 local pc=call(uevr.api,'get_player_controller',0);local isPaused=paused(pc)
 if isPaused~=true then tutorialState.get(pc,'',isPaused,S.ticks)end
 local edge=S.paused~=isPaused;S.paused=isPaused
 if not edge and S.ticks~=1 and isPaused~=true and S.ticks%30~=0 then return end
 local hn=full(call(pc,'GetHUD'))or''
 if S.hud and S.hud~=hn then restoreAll()end;S.hud=hn
 if not hn:match('^LiveMainHUDParent4K_C ')or isPaused~=true then
  restoreAll();return
 end
 local view=tutorialState.get(pc,hn,isPaused,S.ticks);local seen={}
 for _,root in ipairs(view.roots or{})do
  local tutorial=false
  for _,c in ipairs(view.contents or{})do if c.name:find(root.path..'.',1,true)then tutorial=true;break end end
  if tutorial then
   local tree=prop(root.object,'WidgetTree');local tn=full(tree);local path=tn and tn:match('^%S+ (.+)$')
   local function binding(key,cls)return prop(root.object,key)or(path and call(uevr.api,'find_uobject',cls..' '..path..'.'..key))end
   local blur=binding('MaskBackgroundBlur_Bg','BackgroundBlurWithMask')
   local base=binding('Img_Base','LiveNUIImage');local frame=binding('GridPanel_Frame','GridPanel')
   local bn,baseName,fn=full(blur),full(base),full(frame)
   if bn and baseName and fn and full(call(base,'GetParent'))==fn then
    local r=S.records[root.name]
    if r and r.blurName~=bn then restore(root.name);r=nil end
    if not r then
     local visibility=call(blur,'GetVisibility')
     if type(visibility)=='number'then r={rootName=root.name,rootObject=root.object,blurName=bn,blurObject=blur,originalVisibility=visibility};S.records[root.name]=r end
    end
    if r then
     seen[root.name]=true;hide(r)

    end
   end
  end
 end
 local names={};for n in pairs(S.records)do if not seen[n]then names[#names+1]=n end end
 for _,n in ipairs(names)do restore(n)end
end)
pcall(function()uevr.sdk.callbacks.on_script_reset(S.stop)end)

end)(AC8Runtime.host('utility','AC8_VR-TutorialFill.lua'),AC8Runtime.json,AC8Runtime.log)
end


end
assert(not registry.sections['pause_attachment'], 'Duplicate AC8 VR section: pause_attachment')
registry.sections['pause_attachment'] = function(R)
-- Flat2VR Modding Community and Discord: preserve the necessary menu
-- attachment from hud_inspect_v129, limited to the current HUD's outer root.
-- Nested Pause/RootMenu children keep their native hierarchy and input route.
do
local R=AC8Runtime
local host=R.host('utility','pause menu attachment')
local S={version=58,epoch=-1};R.pauseMenuAttachment=S
local lastPauseGuard=nil
local function call(o,k,...)if not o then return end;local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end end
local function full(o)return call(o,'get_full_name')or''end
local statics=call(uevr.api,'find_uobject','GameplayStatics /Script/Engine.Default__GameplayStatics')
host.sdk.callbacks.on_post_engine_tick(function()
 if lastPauseGuard==nil then -- Reload preferences on script/game restart.
  local ok,settings=pcall(R.read,'AC8_VR-PauseControls.json')
  local enabled=not(ok and type(settings)=='table'and settings.enabled==false)
  if enabled~=lastPauseGuard then host.api:dispatch_custom_event('AC8_PauseTickGuard',enabled and'on'or'off');lastPauseGuard=enabled end
 end
 local c=R.context;if not c.flight then S.epoch=-1;return end
 local paused=call(statics,'IsGamePaused',c.pc)==true
 local changed=S.epoch~=c.epoch or S.paused~=paused
 S.epoch=c.epoch;S.paused=paused
 if not changed and R.tick%30~=1 then return end
 local root=call(c.hud,'get_property','HudMenuWidget');local name=full(root)
 if not name:match('^WBP_MenuGlowWidget_C /Engine/Transient%.')and not name:match('^WBP_RootMenuWidget_C /Engine/Transient%.')then return end
 -- If the game has parented the container already, do not reparent it.
 local attached=call(root,'IsInViewport')
 if attached==false and not call(root,'GetParent')then
  call(root,'AddToViewport');attached=call(root,'IsInViewport')
 end
 
end)
end


end


-- End Core pause and tutorial features.
local required={'core','cutscene_guard','inherited_hud','mission_loading','glow_lifecycle','target_presentation','target_labels','target_animation','target_pins','target_materials','hud_policy','glow_brightness','tutorial_fill','pause_attachment','transitions','aircraft_presets','plane_camera','cinematic','cinematic_ownership','hud_layout','loading_icon','gameplay_camera'}
registry.required=required;registry.started={};registry.status={};registry.dependencies={}
local function initialize(key)
 local ok,err=pcall(registry.sections[key],rawget(_G,'AC8Runtime'))
 if not ok then registry.state='failed';registry.error=key..': '..tostring(err);error(registry.error)end
 registry.started[key]=true;registry.status[key]='started';registry.completed[#registry.completed+1]=key
end
function registry.start(finalize)
 if registry.state=='failed'or registry.state=='starting'or registry.state=='ready'then return end
 if not registry.started.core then initialize('core')end
 if not finalize then return end
 registry.state='starting'
 if not registry.sections.cinematic then
  local keys={UI_OverlayType=true,UI_Distance=true,UI_Size=true,UI_FollowView=true,UI_Y_Offset=true,UI_X_Offset=true,UI_Cylinder_Angle=true,VR_CameraForwardOffset=true,VR_CameraRightOffset=true,VR_CameraUpOffset=true}
  AC8Runtime.recoverSettings('AC8_VR-CinematicUIRestore.json',keys)
  AC8Runtime.recoverSettings('AC8_VR-HangarRestoreV48.json',keys)
 end
 AC8Runtime.recoverSettings('AC8_VR-CockpitScaleRestore.json',{VR_WorldScale=true})
 AC8Runtime.recoverSettings('AC8_VR-FlightUISizeRestore.json',{UI_Size=true})
 AC8Runtime.recoverSettings('AC8_VR-AircraftCameraRestore.json',{VR_CameraForwardOffset=true,VR_CameraRightOffset=true,VR_CameraUpOffset=true})
 if registry.sections.hud_layout or registry.sections.aircraft_presets then
  AC8Runtime.hudDefaults=newHUDDefaultsStore(json);AC8Runtime.hudDefaults:init()
 end
 -- Optional feature files remain independent.

 for _,key in ipairs(required)do
  if not registry.started[key]then
   if registry.sections[key]then initialize(key)else registry.status[key]='file not selected'end
  end
 end
 registry.state='ready'
 
end
registry.start()

