-- Optional aircraft presets: HUD, camera offsets, UI Size and cockpit scale.
-- Remove this file with the game closed to retain only general HUD/camera behavior.
local registry=rawget(_G,'AC8VRModules')
if not registry then registry={sections={},state='waiting',completed={}};rawset(_G,'AC8VRModules',registry)end
local function newAircraftPresetStore(files,defaults)
    local P={file='AC8_VR-AircraftPresets.json',backup='AC8_VR-AircraftPresets.backup.json',cameraRevision=0,message='',lastSource=nil}
    local function copy(v)
        if type(v)~='table'then return v end
        local out={};for k,x in pairs(v)do out[k]=copy(x)end;return out
    end
    local function equal(a,b)
        if type(a)~=type(b)then return false end
        if type(a)~='table'then return a==b end
        for k,v in pairs(a)do if not equal(v,b[k])then return false end end
        for k in pairs(b)do if a[k]==nil then return false end end
        return true
    end
    local function plane(key)return type(key)=='string'and key:match('^BP_PlayerPlane_[%w_]+_C$')~=nil end
    local function parameters(p)
        return type(p)=='table'and type(p.enabled)=='boolean'and type(p.x)=='number'and p.x==p.x and math.abs(p.x)<=4000
            and type(p.y)=='number'and p.y==p.y and math.abs(p.y)<=4000 and type(p.scale)=='number'and p.scale>=.1 and p.scale<=3
    end
    local function layout(v)
        if type(v)~='table'or type(v.groups)~='table'or(v.components~=nil and type(v.components)~='table')then return end
        for _,key in ipairs({'ladder','radar','info'})do if not parameters(v.groups[key])then return end end
        local n=0;for key,p in pairs(v.components or{})do n=n+1;if n>24 or type(key)~='string'or not parameters(p)then return end end
        if v.canvas and(not parameters(v.canvas)or type(v.canvas.extraWidth)~='number'or v.canvas.extraWidth~=v.canvas.extraWidth or v.canvas.extraWidth<0 or v.canvas.extraWidth>4000 or type(v.canvas.extraHeight)~='number'or v.canvas.extraHeight~=v.canvas.extraHeight or v.canvas.extraHeight<0 or v.canvas.extraHeight>4000 or type(v.canvas.inheritClip)~='boolean')then return end
        local out={groups=copy(v.groups),components=copy(v.components or{}),canvas=copy(v.canvas),simpleGroups=copy(v.simpleGroups or{}),simpleTargetBase=copy(v.simpleTargetBase)}
        if out.simpleGroups then
            out.simpleGroups.navigation=nil -- Retired combined mover; ignore legacy saved values.
            for key,p in pairs(out.simpleGroups)do
                if type(p)~='table'or type(p.x)~='number'or p.x~=p.x or math.abs(p.x)>4000 or type(p.y)~='number'or p.y~=p.y or math.abs(p.y)>4000 then return end
            end
        end
        return out
    end
    local function uiSize(v)
        if type(v)~='table'or v.version~=1 or type(v.defaultSize)~='number'or v.defaultSize~=v.defaultSize or v.defaultSize<.01 or v.defaultSize>100 or(v.planes~=nil and type(v.planes)~='table')then return end
        local out={version=1,defaultSize=v.defaultSize,planes={}};local n=0
        for k,x in pairs(v.planes or{})do
            n=n+1;if n>256 or not plane(k)or type(x)~='number'or x~=x or x<.01 or x>100 then return end
            out.planes[k]=x
        end
        return out
    end
    local function camera(v)
        if type(v)~='table'then return end
        local out={}
        for _,k in ipairs({'forward','right','up'})do local n=v[k];if type(n)~='number'or n~=n or math.abs(n)>100 then return end;out[k]=n end
        if v.cockpitSize~=nil then if type(v.cockpitSize)~='number'or v.cockpitSize~=v.cockpitSize or v.cockpitSize<.5 or v.cockpitSize>2 then return end;out.cockpitSize=v.cockpitSize end
        return out
    end
    local function cameras(v)
        if type(v)~='table'or v.mode~='uevr_absolute'or type(v.enabled)~='boolean'or not camera(v.default)or(v.planes~=nil and type(v.planes)~='table')then return end
        local out={mode=v.mode,enabled=v.enabled,default=camera(v.default),planes={}};local n=0
        for k,c in pairs(v.planes or{})do n=n+1;local p=camera(c);if n>256 or not plane(k)or not p then return end;out.planes[k]=p end
        return out
    end
    local function encode(db)
        local out={version=2,enabled=db.perPlaneEnabled,useDefaultHUDForAllPlanes=db.useDefaultForAllPlanes,cameraEnabled=db.cameras.enabled,defaultCamera=copy(db.cameras.default),normalUISize=db.uiSize and db.uiSize.defaultSize or 1,aircraft={}}
        local function row(k)out.aircraft[k]=out.aircraft[k]or{};return out.aircraft[k]end
        for k,v in pairs(db.planes)do row(k).hud=copy(v)end
        for k,v in pairs(db.cameras.planes)do
            local c=copy(v);local size=c.cockpitSize;c.cockpitSize=nil;row(k).camera=c;row(k).cockpitSize=size
        end
        for k,v in pairs(db.uiSize and db.uiSize.planes or{})do row(k).uiSize=v end
        return out
    end
    local function database(v)
        if type(v)=='table'and v.version==2 then
            if type(v.enabled)~='boolean'or type(v.useDefaultHUDForAllPlanes)~='boolean'or type(v.cameraEnabled)~='boolean'or(v.aircraft~=nil and type(v.aircraft)~='table')then return end
            local base=defaults and defaults.db
            if not base then return end
            local old={version=1,perPlaneEnabled=v.enabled,useDefaultForAllPlanes=v.useDefaultHUDForAllPlanes,default=copy(base.default),allPlanes=copy(base.shared),planes={},uiSize={version=1,defaultSize=v.normalUISize,planes={}},cameras={mode='uevr_absolute',enabled=v.cameraEnabled,default=copy(v.defaultCamera),planes={}}}
            local n=0
            for k,row in pairs(v.aircraft or{})do
                n=n+1;if n>256 or not plane(k)or type(row)~='table'then return end
                if row.hud~=nil then old.planes[k]=row.hud end
                if row.camera~=nil then
                    if type(row.camera)~='table'then return end
                    old.cameras.planes[k]=copy(row.camera);old.cameras.planes[k].cockpitSize=row.cockpitSize
                elseif row.cockpitSize~=nil then return end
                if row.uiSize~=nil then old.uiSize.planes[k]=row.uiSize end
            end
            v=old
        end
        -- UEVR serializes empty Lua tables as JSON null. Treat null/missing
        -- dictionary containers as empty, then validate and compare the data.
        if type(v)~='table'or v.version~=1 or(v.perPlaneEnabled~=nil and type(v.perPlaneEnabled)~='boolean')or type(v.useDefaultForAllPlanes)~='boolean'or(v.planes~=nil and type(v.planes)~='table')then return end
        local default=layout(v.default);if not default then return end
        local allPlanes=v.allPlanes and layout(v.allPlanes)
        if v.allPlanes and not allPlanes then return end
        local planes={};local n=0
        for key,p in pairs(v.planes or{})do
            n=n+1;local valid=layout(p);if n>256 or not plane(key)or not valid then return end
            planes[key]=valid
        end
        local cameraData=cameras(v.cameras or{mode='uevr_absolute',enabled=true,default={forward=-18,right=0,up=5},planes={}})
        if not cameraData then return end
        local size=v.uiSize and uiSize(v.uiSize)
        if v.uiSize~=nil and not size then return end
        return{version=1,perPlaneEnabled=v.perPlaneEnabled~=false,useDefaultForAllPlanes=v.useDefaultForAllPlanes,default=default,allPlanes=allPlanes,planes=planes,uiSize=size,cameras=cameraData}
    end
    function P:init(default)
        if self.initialized then return self.db~=nil end;self.initialized=true
        if defaults then defaults:init()end
        local ok,existing=pcall(files.load_file,self.file)
        local valid=ok and database(existing)
        if valid then self.db=valid;return true end
        local good,backup=pcall(files.load_file,self.backup);valid=good and database(backup)
        if valid then self.db=valid;self.message='Recovered HUD presets from backup.';return true end
        local rawOK,raw=pcall(function()return files.read and files.read(self.file)end)
        local present=existing~=nil or(rawOK and type(raw)=='string')or(not ok and(not files.read or not rawOK))
        -- Upgrade input only: current saves always use the prefixed filename.
        if not present then
            local oldOK,old=pcall(files.load_file,'AircraftPresets.json')
            local oldValid=oldOK and database(old)
            if not oldValid then local backupOK,b=pcall(files.load_file,'AircraftPresets.backup.json');oldValid=backupOK and database(b)end
            if oldValid then
                self.db=oldValid;local saved,why=self:persist(oldValid)
                if not saved then self.invalid=true;self.message=why end
                return true
            end
        end
        local legacyOK,legacy=pcall(files.load_file,'AC8_VR-HUDLayouts.json')
        local imported=legacyOK and database(legacy)
        if not imported then
            local base=defaults and defaults.db
            default=layout(default)or(base and layout(base.default))
            if not default then self.initialized=false;self.message='Shared HUD defaults unavailable; files left untouched.';return false end
            imported=database({version=1,perPlaneEnabled=true,useDefaultForAllPlanes=false,default=default,allPlanes=base and base.shared or default,planes={}})
        end
        local readOK,text=pcall(function()return files.read and files.read('AC8_VR-PerPlaneCameraOffsets.lua')end)
        if readOK and type(text)=='string'and text~=''then
            local chunk=load(text,'@legacy-aircraft-cameras','t',{});local good,old=pcall(chunk or function()end)
            local values=good and cameras(old)
            if not values then self.invalid=true;self.message='Invalid legacy aircraft cameras; files preserved.';self.db=imported;return true end
            imported.cameras=values
        end
        if not imported.uiSize then
            local good,oldSize=pcall(files.load_file,'AC8_VR-PlaneUISize.json')
            local validSize=good and uiSize(oldSize)
            if not validSize then local good,b=pcall(files.load_file,'AC8_VR-PlaneUISize.backup.json');validSize=good and uiSize(b)end
            if oldSize~=nil and not validSize then self.invalid=true;self.db=imported;self.message='Legacy UI Size preferences invalid; files preserved.';return true end
            imported.uiSize=validSize or{version=1,defaultSize=1,planes={}}
        end
        if defaults and defaults.db then imported.default=copy(defaults.db.default);imported.allPlanes=copy(defaults.db.shared)end
        self.db=imported
        if present then self.invalid=true;self.message='Invalid aircraft presets; existing files preserved.';return true end
        local saved,why=self:persist(imported)
        if not saved then self.invalid=true;self.message=why end
        return true
    end
    function P:persist(next)
        if self.invalid then return false,self.message end
        local validated=database(next);if not validated then return false,'Invalid HUD preset; nothing saved.'end
        if self.db then
            local good,result=pcall(files.dump_file,self.backup,encode(self.db),2)
            local readOK,read=pcall(files.load_file,self.backup)
            if not good or result==false or not readOK or not equal(database(read),database(self.db))then return false,'Could not verify HUD backup; nothing changed.'end
        end
        local good,result=pcall(files.dump_file,self.file,encode(validated),2)
        local readOK,read=pcall(files.load_file,self.file)
        if not good or result==false or not readOK or not equal(database(read),validated)then
            if self.db then pcall(files.dump_file,self.file,encode(self.db),2)end
            return false,'HUD save failed verification; previous presets retained.'
        end
        self.db=validated;return true
    end
    function P:selected(key)
        if not self.db then return end
        if not self.invalid and self.db.perPlaneEnabled~=false and not self.db.useDefaultForAllPlanes and plane(key)and self.db.planes[key]then return copy(self.db.planes[key]),'current aircraft'end
        if self.db.allPlanes then return copy(self.db.allPlanes),'all planes'end
        return copy(self.db.default),'shipped default'
    end
    function P:setPerPlaneEnabled(enabled)
        if type(enabled)~='boolean'or not self.db then return false,'HUD presets unavailable.'end
        local next=copy(self.db);next.perPlaneEnabled=enabled
        local ok,msg=self:persist(next);if ok then self.force=true end
        return ok,msg or(enabled and'Per-plane settings enabled.'or'Per-plane settings disabled; saved presets retained.')
    end
    function P:saveUISizePreferences(value)
        if not self.db or not uiSize(value)then return false,'UI Size preferences are invalid.'end
        local next=copy(self.db);next.uiSize=uiSize(value)
        return self:persist(next)
    end
    function P:setShared(enabled)
        if type(enabled)~='boolean'or not self.db then return false,'HUD presets unavailable.'end
        local next=copy(self.db);next.useDefaultForAllPlanes=enabled
        local ok,msg=self:persist(next);if ok then self.force=true end;return ok,msg
    end
    function P:savePlane(key,current)
        if self.db and self.db.perPlaneEnabled==false then return false,'Enable per-plane settings before saving an aircraft preset.'end
        if not plane(key)then return false,'Enter an aircraft to save its HUD.'end
        current=layout(current);if not current then return false,'Current HUD layout is invalid.'end
        local next=copy(self.db);next.planes[key]=current
        local ok,msg=self:persist(next);return ok,msg or'Saved and verified HUD for current plane.'
    end
    function P:saveAll(current)
        if not defaults then return false,'Shared HUD defaults unavailable.'end
        local ok,msg=defaults:saveShared(current)
        if ok then self.db.allPlanes=copy(defaults.db.shared);self.force=true end
        return ok,msg
    end
    function P:saveGlobalHUD(current)
        if not defaults then return false,'Shared HUD defaults unavailable.'end
        local ok,msg=defaults:saveGlobal(current)
        if ok then self.db.default=copy(defaults.db.default);self.db.allPlanes=copy(defaults.db.shared)end
        return ok,msg
    end
    function P:saveCameraDefault(current)
        local value=camera(current)
        if not self.db or not value then return false,'Global camera offsets are invalid.'end
        local next=copy(self.db)
        next.cameras.default={forward=value.forward,right=value.right,up=value.up}
        local ok,msg=self:persist(next);if ok then self.cameraRevision=self.cameraRevision+1 end
        return ok,msg or'Global camera default saved; aircraft presets unchanged.'
    end
    function P:saveCamera(key,current)
        if not self.db or self.db.perPlaneEnabled==false or not plane(key)or not camera(current)then return false,'Aircraft camera preset is unavailable or invalid.'end
        local next=copy(self.db);next.cameras.planes[key]=camera(current)
        local ok,msg=self:persist(next);if ok then self.cameraRevision=self.cameraRevision+1 end
        return ok,msg or'Saved and verified cockpit camera for current plane.'
    end
    function P:count()
        local count=0;for _ in pairs(self.db and self.db.planes or{})do count=count+1 end;return count
    end
    P.copy=copy;P.layout=layout;P.plane=plane;P.encode=encode
    return P
end

-- This module owns aircraft UI Size; Core only retains generic crash/removal recovery.
local function newFlightUISize(files,vr,store,enabled)
 local S={store=store,journal='AC8_VR-FlightUISizeRestore.json'}
 local function number(v)v=tonumber(v);return v and v==v and v>=.01 and v<=100 and v or nil end
 local function same(a,b)return a and b and math.abs(a-b)<1e-5*math.max(1,math.abs(b))end
 local function clone(v)if type(v)~='table'then return v end;local t={};for k,x in pairs(v)do t[k]=clone(x)end;return t end
 local function plane(k)return type(k)=='string'and k:match('^BP_PlayerPlane_[%w_]+_C$')~=nil end
 local function valid(v)
  if type(v)~='table'or v.version~=1 or not number(v.defaultSize)or(v.planes~=nil and type(v.planes)~='table')then return end
  local out={version=1,defaultSize=number(v.defaultSize),planes={}};local n=0
  for k,x in pairs(v.planes or{})do n=n+1;if n>256 or not plane(k)or not number(x)then return end;out.planes[k]=number(x)end
  return out
 end
 function S.read()local ok,v=pcall(function()return vr:get_mod_value('UI_Size')end);return ok and number(v)or nil end
 local function write(v)
  if same(S.read(),v)then return true end
  local ok=pcall(function()vr.set_mod_value('UI_Size',tostring(v))end);return ok and same(S.read(),v)
 end
 local function persist(next)
  if S.invalid or not S.store or not S.store.db then return false,'HUD preferences unavailable; UI Size unchanged.'end
  local ok,why=S.store:saveUISizePreferences(next)
  if ok then S.db=valid(S.store.db.uiSize)end
  return ok,why
 end
 local function init(actual)
  if S.initialized then return S.db~=nil and not S.invalid end
  if not S.store or not S.store.db then return false end
  S.initialized=true
  local v=S.store.db.uiSize;local db=v and valid(v)
  if v~=nil and not db then S.invalid=true;S.message='Invalid UI Size preferences; native settings preserved.';return false end
  if db then S.db=db end
  local next=db and clone(db)or{version=1,defaultSize=actual,planes={}}
  next.defaultSize=actual
  if not v or not db or not same(db.defaultSize,actual)then local good,why=persist(next);if not good then S.message=why;return false end end
  return true
 end
 local function journal(value)
  local values=value and{UI_Size=value}or{}
  local ok,result=pcall(files.dump_file,S.journal,{version=1,values=values},2);local readOK,v=pcall(files.load_file,S.journal)
  return ok and result~=false and readOK and type(v)=='table'and
   (value and type(v.values)=='table'and same(number(v.values.UI_Size),value)or not value and(v.values==nil or type(v.values)=='table'and next(v.values)==nil))
 end
 function S.restore()
  if not S.owned then S.currentPlane=nil;S.selection=nil;return true end
  if not write(S.db.defaultSize)then S.message='Normal UI Size could not be restored; recovery retained.';return false end
  -- Persist restoration so saving UEVR config during flight cannot leak the plane size into the next launch.
  local ok,result=pcall(function()return vr.save_config()end)
  if not ok or result==false or not journal(nil)then S.message='Normal UI Size restored; persistence recovery remains pending.';return false end
  S.owned=nil;S.currentPlane=nil;S.selection=nil;return true
 end
 function S.update(key)
  if enabled and not enabled()then key=nil end
  if S.recoveryFailed then return end
  if S.owned and plane(key)and S.selection==key then return end
  local actual=S.read();if not actual or not init(actual)then return end
  if not plane(key)then
   if not S.restore()then return end
   actual=S.read()
   if actual and not same(actual,S.db.defaultSize)then local next=clone(S.db);next.defaultSize=actual;local ok,why=persist(next);if not ok then S.message=why end end
   return
  end
  if not S.owned then
   if not journal(S.db.defaultSize)then S.message='Normal UI Size recovery could not be verified; flight override skipped.';return end
   S.owned=true
  end
  S.currentPlane=key
  if S.selection~=key then
   local desired=S.db.planes[key]or S.db.defaultSize
   if not write(desired)then S.message='Aircraft UI Size could not be applied; recovery retained.';return end
   S.selection=key
  end
 end
 function S.savePlane(key)
  if enabled and not enabled()then return false,'Enable per-plane settings before saving UI Size.'end
  if not S.db or not plane(key)or S.currentPlane~=key then return false,'Enter flight in the selected aircraft before saving UI Size.'end
  local v=S.read();if not v then return false,'UEVR UI Size is unavailable.'end
  local next=clone(S.db);next.planes[key]=v;local ok,why=persist(next)
  if ok then S.selection=key;S.message='Saved UI Size for current plane.'end
  return ok,why or S.message
 end
 function S.useDefault(key)
  if enabled and not enabled()then return false,'Enable per-plane settings before changing an aircraft preset.'end
  if not S.db or not plane(key)or S.currentPlane~=key then return false,'Enter flight before resetting this aircraft UI Size.'end
  local next=clone(S.db);next.planes[key]=nil;local ok,why=persist(next)
  if ok then S.selection=nil;S.update(key);S.message='This plane now uses your normal UI Size.'end
  return ok,why or S.message
 end
 return S
end
-- End flight UI Size owner.

local function attachAircraftUI(owner,R)
 local hudProfiles=owner
 local function profilePlane()
  if not R.context.flight then return end
  return R.viewTargetName():match('^(BP_PlayerPlane_[%w_]+_C) ')
 end
 local function queueProfileAction(kind,value)
  local controls=R.hudLayoutControls
  owner.pending={kind=kind,plane=profilePlane(),layout=controls and controls.snapshot()or nil,value=value}
 end
 function owner.processUIActions()
  local action=owner.pending;owner.pending=nil;if not action then return end
  local plane=profilePlane();local ok,msg=true,nil
  if action.kind=='perPlaneMode'then
   ok,msg=owner:setPerPlaneEnabled(action.value)
   if ok then R.perPlaneSettingsEnabled=owner.db.perPlaneEnabled and not owner.invalid;R.flightUISize.update(R.perPlaneSettingsEnabled and plane or nil)end
  elseif action.kind=='mode'then ok,msg=owner:setShared(action.value)
  elseif action.kind=='reset'then owner.force=true;msg='Reset to the selected saved HUD layout.'
  elseif action.kind=='all'then ok,msg=owner:saveAll(action.layout)
  elseif action.kind=='hudGlobal'then ok,msg=owner:saveGlobalHUD(action.layout)
  elseif action.kind=='cameraGlobal'then
   if not plane or plane~=action.plane then ok,msg=false,'Enter flight in the same aircraft before saving the global camera.'
   elseif not R.planeSpecificOffset then ok,msg=false,'Aircraft camera owner unavailable.'
   else ok,msg=R.planeSpecificOffset.saveGlobalDefault(plane)end
  elseif not owner.db or owner.db.perPlaneEnabled==false then ok,msg=false,'Enable per-plane settings before changing or saving an aircraft preset.'
  elseif not plane or plane~=action.plane then ok,msg=false,'Aircraft changed; nothing saved.'
  elseif action.kind=='uiSize'then ok,msg=R.flightUISize.savePlane(plane)
  elseif action.kind=='uiSizeDefault'then ok,msg=R.flightUISize.useDefault(plane)
  elseif action.kind=='plane'then ok,msg=owner:savePlane(plane,action.layout)
  else
   local camera=R.planeSpecificOffset
   if not camera then ok,msg=false,'Aircraft camera owner unavailable.'
   else
    ok,msg=camera.saveCurrentPlane(plane)
    if ok and action.kind=='both'then
     local saved,why=owner:savePlane(plane,action.layout)
     ok=saved;msg=saved and'Saved and verified HUD + cockpit camera for current plane.'or('Camera saved; '..tostring(why))
    end
   end
  end
  if ok and(action.kind=='plane'or action.kind=='both')then
   local saved,why=R.flightUISize.savePlane(plane)
   if not saved then ok=false;msg=tostring(msg)..' UI Size was not saved: '..tostring(why)else msg=tostring(msg)..' UI Size saved too.'end
  end
  if ok and R.hudLayoutControls and(action.kind=='plane'or action.kind=='all'or action.kind=='both'or action.kind=='hudGlobal')then R.hudLayoutControls.saved()end
  owner.message=msg or(ok and'HUD layout selection changed.'or'Nothing saved.')
 end
local function uiSection(label)
    if imgui.spacing then imgui.spacing()end
    if imgui.separator then imgui.separator()else imgui.text('--------------------------------')end
    if label then imgui.text(label)end
    if imgui.spacing then imgui.spacing()end
end
owner.drawUI=function()
    if not imgui then return end
    hudProfiles.currentPlane=profilePlane()
    uiSection('Aircraft presets')
    if not hudProfiles.db then imgui.text('Enter flight once to initialize HUD presets.');return end
    if R.aircraftPresets then
        local masterChanged,masterValue=imgui.checkbox('Enable per-plane settings',hudProfiles.db.perPlaneEnabled~=false)
        if masterChanged then queueProfileAction('perPlaneMode',masterValue)end
    else imgui.text('AC8_VR-AircraftPresets.lua is absent. Shared HUD controls remain available.')end
    if hudProfiles.db.perPlaneEnabled==false then imgui.text('Per-plane overrides are off. Saved presets are retained; general HUD controls remain available.')end
    if R.aircraftPresets then
        local changed,value=imgui.checkbox('Use default HUD layout for all planes',hudProfiles.db.useDefaultForAllPlanes)
        if changed then queueProfileAction('mode',value)end
    end
    local plane=hudProfiles.currentPlane
    local label=plane and(plane:match('^BP_PlayerPlane_PP%d+_(.-)_C$')or plane):upper()or'No active aircraft'
    imgui.text('Current aircraft: '..label)
    imgui.text('HUD source: '..(hudProfiles.lastSource or'all planes')..' | Aircraft HUD presets: '..hudProfiles:count())
    uiSection('Global defaults')
    if R.hudLayoutControls and imgui.button('Save current HUD as global default')then queueProfileAction('hudGlobal')end
    if imgui.button('Save current camera as global default')then queueProfileAction('cameraGlobal')end
    imgui.text('Global HUD updates the fallback/shared layout. Camera saves position only.')
    imgui.text('Saved aircraft presets are preserved.')
    uiSection('HUD layout saves')
    if R.hudLayoutControls and imgui.button('Save HUD layout for current plane')then queueProfileAction('plane')end
    if R.hudLayoutControls and imgui.button('Save HUD layout for ALL planes')then queueProfileAction('all')end
    if imgui.spacing then imgui.spacing()end
    if R.hudLayoutControls and imgui.button('Reset HUD to saved layout')then queueProfileAction('reset')end
    if R.aircraftPresets then
        uiSection('Camera and combined saves')
        if imgui.button('Save cockpit camera for current plane')then queueProfileAction('camera')end
        if R.hudLayoutControls and imgui.button('Save HUD + cockpit camera for current plane')then queueProfileAction('both')end
    end
    imgui.text('Shared and aircraft layouts are separate. Reset uses the selected mode and saved fallbacks.')
    if hudProfiles.message~=''then imgui.text(hudProfiles.message)end
    local uiSize=R.flightUISize
    if uiSize then
        uiSection('Flight UI Size')
        local current=uiSize.read()
        imgui.text('Current UI Size: '..(current and string.format('%.3g',current)or'unavailable'))
        imgui.text('Normal UI Size: '..(uiSize.db and string.format('%.3g',uiSize.db.defaultSize)or'waiting'))
        if imgui.button('Save UI Size for current plane')then queueProfileAction('uiSize')end
        if imgui.button('Use normal UI Size for current plane')then queueProfileAction('uiSizeDefault')end
        imgui.text('Adjust UI Size in the UEVR menu. HUD saves for this plane include it too.')
        imgui.text('Independent of shared HUD layout. Pause/retry keep the flight size; other modes use normal size.')
        if uiSize.message then imgui.text(uiSize.message)end
    end
    local camera=R.planeSpecificOffset
    if camera and camera.getCockpitSize and plane and hudProfiles.db.perPlaneEnabled~=false then
        uiSection('Cockpit size (cockpit view only)')
        local enabled,size=camera.getCockpitSize(plane)
        local changed,value=imgui.checkbox('Customize cockpit size for this plane',enabled)
        if changed then enabled=value;camera.setCockpitSize(plane,enabled,size)end
        if enabled then
            local edited,nextSize=imgui.slider_float('Cockpit size multiplier',size,.5,2)
            if edited then size=nextSize;camera.setCockpitSize(plane,true,size)end
            if imgui.button('Cockpit size -0.01')then camera.setCockpitSize(plane,true,math.max(.5,size-.01))end
            if imgui.same_line then imgui.same_line()end
            if imgui.button('Cockpit size +0.01')then camera.setCockpitSize(plane,true,math.min(2,size+.01))end
            if imgui.button('Reset cockpit size to 1.0x')then camera.setCockpitSize(plane,true,1)end
        end
        imgui.text('Higher = larger pilot/cockpit/world. Save with the cockpit camera button above.')
        if camera.cockpitSize and camera.cockpitSize.message then imgui.text(camera.cockpitSize.message)end
    end
    if imgui.separator then imgui.separator()end
end

end
-- End aircraft preset UI.

assert(not registry.sections.aircraft_presets,'Duplicate AircraftPresets module')
registry.sections.aircraft_presets=function(R)
 local owner=newAircraftPresetStore({load_file=json.load_file,dump_file=json.dump_file,read=fs and fs.read},R.hudDefaults)
 owner:init(nil);R.aircraftPresets=owner;R.hudProfiles=owner
 R.perPlaneSettingsEnabled=owner.db and owner.db.perPlaneEnabled~=false and not owner.invalid
 R.flightUISize=newFlightUISize(json,uevr.params.vr,owner,function()return R.perPlaneSettingsEnabled~=false end)
 R.flightUISize.recoveryFailed=R.errors and R.errors['AC8_VR-FlightUISizeRestore.json']~=nil
 local host=R.host('presentation','aircraft UI Size')
 host.sdk.callbacks.on_pre_engine_tick(function()
  R.perPlaneSettingsEnabled=owner.db and owner.db.perPlaneEnabled~=false and not owner.invalid
  local key
  if R.context.flight and not R.settingsBusy('presentation','gameplay')and R.matchState()~='WaitingToStart'then key=R.viewTargetName():match('^(BP_PlayerPlane_[%w_]+_C) ')end
  R.flightUISize.update(key)
 end)
 attachAircraftUI(owner,R)
 host.sdk.callbacks.on_post_engine_tick(owner.processUIActions)
 host.sdk.callbacks.on_draw_ui(owner.drawUI)
 host.sdk.callbacks.on_script_reset(R.flightUISize.restore)
end

assert(not registry.sections['plane_camera'], 'Duplicate AC8 VR section: plane_camera')
registry.sections['plane_camera'] = function(R)
local cockpitSizeFiles=json
assert(R.aircraftPresets,'Aircraft preset owner unavailable')
do
-- Absolute UEVR settings, applied on aircraft/configuration/ownership changes.
-- No stereo pose callbacks and no save_config calls.
local R=AC8Runtime
local S={version=92,config={enabled=false,default={forward=-18,right=0,up=5},planes={}},nextRead=0,revision=0,status={state='waiting'}}
R.planeSpecificOffset=S
local host=R.host('utility','plane specific UEVR camera')
local keys={forward='VR_CameraForwardOffset',right='VR_CameraRightOffset',up='VR_CameraUpOffset'}
local axes={'forward','right','up'}
local function call(o,k,...)
 if not o or not R.isLiveObject(o)then return end
 local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end
end
local function row(v)
 if type(v)~='table'then return end
 local result={}
 for _,axis in ipairs(axes)do local n=v[axis];if type(n)~='number'or n~=n or math.abs(n)>100 then return end;result[axis]=n end
 if v.cockpitSize~=nil then
  if type(v.cockpitSize)~='number'or v.cockpitSize~=v.cockpitSize or v.cockpitSize<.5 or v.cockpitSize>2 then return end
  result.cockpitSize=v.cockpitSize
 end
 return result
end
local function readConfig()
 local owner=R.aircraftPresets
 if not owner or not owner.db then S.configError='Aircraft presets unavailable';return end
 if S.storeRevision==owner.cameraRevision then return end
 S.storeRevision=owner.cameraRevision;S.config=owner.copy(owner.db.cameras);S.configLoaded=true;S.configError=nil;S.revision=S.revision+1
end
local function observed()
 local result={}
 for _,axis in ipairs(axes)do
  local ok,v=pcall(function()return uevr.params.vr:get_mod_value(keys[axis])end)
  local n=ok and tonumber(v);if not n or n~=n or math.abs(n)>1e9 then return end
  result[axis]=n
 end
 return result
end
local function cameraRecoveryWrite(normal)
 local values={};for _,axis in ipairs(axes)do values[keys[axis]]=normal[axis]end
 local ok,result=pcall(cockpitSizeFiles.dump_file,'AC8_VR-AircraftCameraRestore.json',{version=1,values=values},2)
 local readOK,saved=pcall(cockpitSizeFiles.load_file,'AC8_VR-AircraftCameraRestore.json')
 if not ok or result==false or not readOK or type(saved)~='table'or type(saved.values)~='table'then return false end
 for k,v in pairs(values)do if tonumber(saved.values[k])~=v then return false end end
 return true
end
local function apply(value)
 local before=observed();if not before then return false,'UEVR camera settings unavailable'end
 if not S.cameraRecovery then
  if not cameraRecoveryWrite(S.config.default)then return false,'Camera recovery could not be verified'end
  S.cameraRecovery=true
 end
 for _,axis in ipairs(axes)do
  if before[axis]~=value[axis]then
   local ok=pcall(function()uevr.params.vr.set_mod_value(keys[axis],tostring(value[axis]))end)
   
   local actual=observed()
   if not ok or not actual or actual[axis]~=value[axis]then
    for _,restoreAxis in ipairs(axes)do pcall(function()uevr.params.vr.set_mod_value(keys[restoreAxis],tostring(before[restoreAxis]))end)end
    return false,'UEVR write/readback failed; previous values restored where possible'
   end
  end
 end
 return true
end
local function presentationOwns(ci)
 return R.settingsBusy('camera','plane')or ci and(ci.maskActive or ci.hangarActive or ci.cameraLocked or ci.shotOverridesActive or ci.guardForced or ci.recoveringUI or
  (ci.saved and next(ci.saved)~=nil)or(ci.hangarSaved and next(ci.hangarSaved)~=nil)or ci.detectedMode=='cinematic'or ci.detectedMode=='hangar')
end
-- Executable-verified view selection: 0 = first person, 1 = cockpit,
-- 2 = third person. The three transition bodies write +0x108 and broadcast
-- OnCameraViewChanged. Component IsActive is not this selected-view enum.
local function selected(value)
 if type(value)=='number'and value==math.floor(value)and value>=0 and value<=5 then return value end
end
local function viewChanged(_,object,locals)
 if S.stopped or object~=S.viewOwner or not R.isLiveObject(object)then return end
 local success,value=pcall(function()return locals.CameraViewType end)
 if success and selected(value)~=nil then
  S.selectedView=value;S.viewEventRevision=(S.viewEventRevision or 0)+1
  S.viewSource='game OnCameraViewChanged';
 end
end

local function ensureViewHook()
 if S.viewHook or R.tick<(S.hookRetryAt or 0)then return end
 S.hookRetryAt=R.tick+60
 local ok,fn=pcall(function()return uevr.api:find_uobject('Function /Script/Live.LivePlayerPlane.OnCameraViewChanged')end)
 if not ok or not fn then return end
 local good,result=pcall(function()fn=fn:as_function();return fn:hook_ptr(nil,viewChanged)end)
 if good and result~=false then S.viewHook=fn end
end
host.sdk.callbacks.on_lua_event(function(event,data)
 if S.stopped or event~='AC8_PlaneViewBootstrapResult'or type(data)~='string'then return end
 local serial,value=data:match('^(%d+)|(-?%d+)$');serial=tonumber(serial);value=tonumber(value)
 if serial~=S.bootstrapSerial or S.bootstrapRevision~=(S.viewEventRevision or 0)then return end
 if selected(value)~=nil then S.selectedView=value;S.viewSource='verified selected-view bootstrap' end
end)
local function currentView(target,targetName)
 ensureViewHook()
 if S.viewOwner~=target or S.viewOwnerName~=targetName then
  S.viewOwner=target;S.viewOwnerName=targetName;S.viewComponent=nil;S.viewResolveAt=0;S.selectedView=nil;S.viewSource=nil
  S.viewEventRevision=(S.viewEventRevision or 0)+1
 end
 local view=S.viewComponent
 if view and not R.isLiveObject(view)then S.viewComponent=nil;view=nil;S.selectedView=nil;S.viewResolveAt=0 end
 if not view and R.tick>=(S.viewResolveAt or 0)then
  S.viewResolveAt=R.tick+60
  if not R.isLiveObject(S.cameraViewClass)then
   local ok,cls=pcall(function()return uevr.api:find_uobject('Class /Script/Live.LiveCameraViewComponent')end)
   S.cameraViewClass=ok and cls or nil
  end
  local found=R.isLiveObject(S.cameraViewClass)and call(target,'GetComponentByClass',S.cameraViewClass)or nil
  local n=call(found,'get_full_name')or''
  if n:match('^LiveCameraViewComponent ')and call(found,'GetOwner')==target then
   S.viewComponent=found;view=found;S.viewComponentName=n
   S.bootstrapSerial=(S.bootstrapSerial or 0)+1;S.bootstrapRevision=S.viewEventRevision or 0
   pcall(function()uevr.api:dispatch_custom_event('AC8_PlaneViewBootstrap',tostring(S.bootstrapSerial)..'|'..n)end)
  end
 end
 -- If the hook is unavailable, periodically re-bootstrap instead of using
 -- component flags. Normal supported operation performs no selected-view polling.
 if view and not S.viewHook and R.tick>=(S.bootstrapRetryAt or 0)then
  S.bootstrapRetryAt=R.tick+60;S.bootstrapSerial=(S.bootstrapSerial or 0)+1;S.bootstrapRevision=S.viewEventRevision or 0
  pcall(function()uevr.api:dispatch_custom_event('AC8_PlaneViewBootstrap',tostring(S.bootstrapSerial)..'|'..S.viewComponentName)end)
 end
 local value=S.selectedView
 return value==2,{selectedView=value,selectedViewName=({[0]='first-person',[1]='cockpit',[2]='third-person'})[value]or'unknown/special',
  viewComponent=S.viewComponentName,confirmedThirdPerson=value==2,firstPersonAllowed=value==0 or value==1,
  detection=S.viewSource or'waiting for selected view',eventHookInstalled=S.viewHook~=nil}
end

-- Per-aircraft perceived-size adjustment through UEVR's existing world scale.
-- Projection matrices and headset FOV remain owned by UEVR/the VR runtime.
local sizeState={edits={}};S.cockpitSize=sizeState
local sizeJournal='AC8_VR-CockpitScaleRestore.json'
local function sizeValue(v)
    v=tonumber(v);return v and v==v and v>=.01 and v<=10 and v or nil
end
local function sizeRead()
    local ok,v=pcall(function()return uevr.params.vr:get_mod_value('VR_WorldScale')end)
    return ok and sizeValue(v)or nil
end
local function nearScale(a,b)return a and b and math.abs(a-b)<1e-5*math.max(1,math.abs(b))end
local function sizeWrite(v)
    local ok=pcall(function()uevr.params.vr.set_mod_value('VR_WorldScale',tostring(v))end)
    return ok and nearScale(sizeRead(),v)
end
local function sizeJournalWrite(value)
    if not cockpitSizeFiles or not cockpitSizeFiles.dump_file or not cockpitSizeFiles.load_file then return false end
    local values=value and{VR_WorldScale=value}or{}
    local ok,result=pcall(cockpitSizeFiles.dump_file,sizeJournal,{version=1,values=values},2)
    local readOK,data=pcall(cockpitSizeFiles.load_file,sizeJournal)
    if not ok or result==false or not readOK or type(data)~='table'then return false end
    if value then return type(data.values)=='table'and nearScale(tonumber(data.values.VR_WorldScale),value)end
    return data.values==nil or type(data.values)=='table'and next(data.values)==nil
end
local function sizePreference(class)
    if R.perPlaneSettingsEnabled==false then return end
    if sizeState.edits[class]then return sizeState.edits[class].value end
    local saved=S.config.planes[class];return saved and saved.cockpitSize
end
function S.getCockpitSize(class)
    if not class then return false,1 end
    local value=sizePreference(class);return value~=nil,value or 1
end
function S.setCockpitSize(class,enabled,value)
    if type(class)~='string'or not class:match('^BP_PlayerPlane_[%w_]+_C$')or type(enabled)~='boolean'or
        type(value)~='number'or value~=value or value<.5 or value>2 then return false end
    sizeState.edits[class]={value=enabled and value or nil};sizeState.selection=nil;return true
end
local function sizeRestore()
    if not sizeState.before then sizeState.selection=nil;return true end
    local actual=sizeRead()
    if not actual then sizeState.message='World scale unavailable; restoration remains pending.';return false end
    -- Respect a manual UEVR edit made after our last write.
    if(sizeState.failed or nearScale(actual,sizeState.written))and not sizeWrite(sizeState.before)then
        sizeState.message='Could not restore normal world scale; recovery retained.';return false
    end
    if not sizeJournalWrite(nil)then sizeState.message='Normal scale restored; recovery journal is still pending.';return false end
    sizeState.before=nil;sizeState.written=nil;sizeState.failed=nil;sizeState.selection=nil;return true
end
local function updateCockpitSize(class,view)
    local value=class and sizePreference(class)
    if not S.config.enabled or not value or value==1 or not view or view.selectedView~=1 then sizeRestore();return end
    local selection=class..':'..tostring(value)
    if sizeState.selection==selection then return end
    local before=sizeState.before or sizeRead()
    if not before then sizeState.message='World scale is unavailable.';return end
    if not sizeState.before then
        if not sizeJournalWrite(before)then sizeState.message='Could not verify normal-scale recovery; size unchanged.';return end
        sizeState.before=before
    end
    local desired=before/value -- A larger size multiplier makes the world appear larger.
    if not sizeValue(desired)or not sizeWrite(desired)then
        sizeState.failed=true;sizeState.message='Cockpit size could not be applied; normal-scale recovery retained.';return
    end
    sizeState.written=sizeRead();sizeState.selection=selection
    sizeState.message='Cockpit size applied; other views use normal world scale.'
end
if R.errors and R.errors[sizeJournal]then sizeState.recoveryFailed=true;sizeState.message='Normal-scale recovery failed; cockpit size remains disabled.'end


-- Explicit user save. The existing camera owner remains optional.
function S.saveCurrentPlane(expected)
    if R.perPlaneSettingsEnabled==false then return false,'Enable per-plane settings before saving the camera.'end
    if S.stopped or not R.context.flight or R.matchState()=='WaitingToStart'or presentationOwns(rawget(_G,'AC8_CinematicIntent'))then return false,'Enter cockpit flight before saving the camera.'end
    local target=call(R.context.pc,'GetViewTarget');local name=call(target,'get_full_name')or''
    local key=name:match('^(BP_PlayerPlane_[%w_]+_C) ')
    if not key or key~=expected then return false,'Aircraft changed; camera was not saved.'end
    local _,view=currentView(target,name)
    if not view or not view.firstPersonAllowed then return false,'Use a confirmed cockpit or first-person view before saving.'end
    local value=row(observed());if not value then return false,'Camera offsets must be valid values between -100 and 100.'end
    value.cockpitSize=sizePreference(key)
    local owner=R.aircraftPresets
    local ok,msg=owner:saveCamera(key,value)
    if ok then S.storeRevision=nil;readConfig();S.selection=nil end
    return ok,msg

end

function S.saveGlobalDefault(expected)
    readConfig()
    if S.stopped or not R.context.flight or R.matchState()=='WaitingToStart'or presentationOwns(rawget(_G,'AC8_CinematicIntent'))then return false,'Enter cockpit flight before saving the global camera.'end
    local target=call(R.context.pc,'GetViewTarget');local name=call(target,'get_full_name')or''
    local key=name:match('^(BP_PlayerPlane_[%w_]+_C) ')
    if not key or key~=expected then return false,'Aircraft changed; global camera was not saved.'end
    local _,view=currentView(target,name)
    if not view or not view.firstPersonAllowed then return false,'Use a confirmed cockpit or first-person view before saving.'end
    local value=row(observed());if not value then return false,'Global camera offsets must be valid values between -100 and 100.'end
    local before=S.config.default
    if not cameraRecoveryWrite(value)then cameraRecoveryWrite(before);return false,'Global camera recovery could not be verified; default unchanged.'end
    local owner=R.aircraftPresets;local ok,msg=owner:saveCameraDefault(value)
    if not ok then cameraRecoveryWrite(before);return false,msg end
    S.cameraRecovery=true
    local previous,revision=S.selection,S.revision;S.storeRevision=nil;readConfig()
    -- Retain the current manually tuned pose; the next aircraft/view transition selects its saved/default pose.
    if previous then S.selection=previous:gsub(':'..revision..':',':'..S.revision..':',1)end
    return true,msg
end

local function update()
 if S.lastTick==R.tick then return end;S.lastTick=R.tick
 readConfig()
 local status={mode='uevr_absolute',state='waiting',configError=S.configError};S.status=status
 local ci=rawget(_G,'AC8_CinematicIntent')
 if presentationOwns(ci)then sizeRestore();S.selection=nil;status.state='cinematic/hangar owns camera';return end
 local target=call(R.context.pc,'GetViewTarget')
 if not target then target=call(call(R.context.pc,'get_property','PlayerCameraManager'),'GetViewTarget')end
 local name=call(target,'get_full_name')or''
 local class=name:match('^(BP_PlayerPlane_[%w_]+_C) ')
 local third,viewInfo=false,nil
 if R.context.flight and class and R.matchState()~='WaitingToStart'then third,viewInfo=currentView(target,name)end
 if not sizeState.recoveryFailed then updateCockpitSize(class,viewInfo)end
 local admitted=viewInfo and viewInfo.firstPersonAllowed==true
 status.aircraftClass=class;status.viewTarget=name;status.viewDetection=viewInfo
 status.selectionReason=admitted and'confirmed first-person/cockpit view'or third and'third-person reset'or'outside confirmed first-person view'
 local desired={forward=0,right=0,up=0};local selection='outside-first-person'
 if third then selection='third-person'
 elseif admitted then
  desired=S.config.default;selection='default'
  if R.perPlaneSettingsEnabled~=false and S.config.enabled and S.config.planes[class]then desired=S.config.planes[class];selection=class end
 end
 if not S.config.enabled and not S.owned then status.state='disabled';return end
 local key=selection..':'..S.revision..':'..(admitted and name or '')..':'..tostring(viewInfo and viewInfo.selectedView)
 if S.selection~=key then
  if R.tick<(S.retryAt or 0)then status.state='waiting to retry UEVR write';return end
  local ok,err=apply(desired)
  if not ok then S.retryAt=R.tick+60;status.state='write failed';status.error=err;return end
  S.retryAt=nil;S.selection=key;S.owned=S.config.enabled;status.appliedThisTick=true
 end
 status.state=not admitted and'outside first-person; zero UEVR offset'or S.config.enabled and(selection=='default'and'default UEVR position'or'aircraft UEVR position')or'disabled; default restored'
 S.thirdPerson=third;S.firstPersonAllowed=admitted
 status.forward=desired.forward;status.right=desired.right;status.up=desired.up
 
 -- Manual menu tuning is allowed until the next
 -- aircraft change, configuration edit or return from cinematic/hangar control.
end
host.sdk.callbacks.on_post_engine_tick(update)
host.sdk.callbacks.on_script_reset(function()
 sizeRestore();S.stopped=true
 if S.owned and not presentationOwns(rawget(_G,'AC8_CinematicIntent'))then
  local restored=apply(S.firstPersonAllowed and S.config.default or{forward=0,right=0,up=0})
  if restored then local ok,result=pcall(function()return uevr.params.vr.save_config()end);if ok and result~=false then pcall(cockpitSizeFiles.dump_file,'AC8_VR-AircraftCameraRestore.json',{version=1,values={}},2)end end
 end
end)
end

end



if registry.start then registry.start()end
