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
assert(not registry.sections['glow_lifecycle'], 'Duplicate AC8 VR section: glow_lifecycle')
registry.sections['glow_lifecycle'] = function(R)
local tutorialState=R.ensureTutorialState()
do
-- ac8 glow lifecycle
(function(uevr,json,print)

-- A flight HUD can remain alive throughout IGC movies. Admission must follow
-- the current gameplay view target, rather than the mere presence of that HUD.
R.glowGameplayAllowed=false;R.glowGameplayReason='waiting for gameplay'

local lastFlight,lastPaused=nil,nil
local lastMission,lastGameplayLevel=nil,nil
local statics=nil
local function invoke(o,k,...)
    if not o then return nil end
    local ok,v=pcall(function(...) return o[k](o,...) end,...)
    if ok then return v end
end
local function dispatch(data)
    pcall(function() uevr.api:dispatch_custom_event('AC8_HUD_GlowLifecycle',data) end)
end
local function level(name)
    
    return name:match('^%S+ (.+%.PersistentLevel)%.') or name:match('^%S+ (.+)%.[^%.]+$')
end
local function update(post)
    local context=R.context
    local pc=context and context.pc or invoke(uevr.api,'get_player_controller',0)
    local hud=context and context.hud or invoke(pc,'GetHUD')
    local name=context and context.hudName or invoke(hud,'get_full_name')or''
    local paused=R.paused()
    local actualFlight=name:match('^LiveMainHUDParent4K_C ')~=nil
    local hudLevel=level(name)
    local targetName=R.viewTargetName();local cinematic=movie(R)
    local state=R.matchState()
    local reason='outside flight HUD'
    local flight=false
    if actualFlight then
        if type(paused)~='boolean'then reason='pause state unavailable'
        elseif state=='WaitingToStart' then reason='mission loading'
        elseif cinematic then reason='cinematic or hangar'
        elseif state~=nil and state~='InProgress'then reason='waiting for confirmed mission'
        else
            -- Two bounded owner reads, no object enumeration or widget walk.
            -- Includes cockpit/chase camera modes belonging to the player plane.
            -- Current owner name is shared for this phase; no enumeration.
            flight=targetName:match('^BP_PlayerPlane_')~=nil
            reason=flight and 'player plane gameplay' or 'non-gameplay view target'
        end
    end
    -- Pause UI in the same mission retains authored glow. A movie/hangar
    -- still owns presentation, including when it is paused.
    if not actualFlight and paused==true and lastGameplayLevel and
       (name==''or hudLevel==lastGameplayLevel)and state~='WaitingToStart' and
       not cinematic and targetName:match('^BP_PlayerPlane_')~=nil then
        flight=true;reason='in-mission pause UI'
    end
    if flight and actualFlight and paused==false then lastGameplayLevel=hudLevel end
    local tutorial=post and tutorialState.get(pc,name,paused,R.tick)or tutorialState
    if flight and tutorial.visible then flight=false;reason='tutorial presentation'end
    local cover=rawget(_G,'AC8VRTransitions')
    if (R.missionLoading and R.missionLoading.active==true)or(cover and not cover.stopped and(cover.loading or(cover.opacity or 0)>0))then
        flight=false;reason='loading/menu cover owns presentation'
    end
    R.glowGameplayAllowed=flight;R.glowGameplayReason=reason
    if flight~=lastFlight then dispatch(flight and 'flight' or 'outside_flight');lastFlight=flight end
    if actualFlight then
        local controller=invoke(pc,'get_full_name')
        local mission=controller and controller..'|'..(hudLevel or name)
        if mission and mission~=lastMission then lastMission=mission;dispatch('mission_entry') end
    elseif name:match('^BP_LiveMenuHUD_C ') and paused~=true then
        lastMission,lastGameplayLevel=nil,nil
    end
    if type(paused)=='boolean' then
        if lastPaused~=nil and paused~=lastPaused and flight then dispatch(paused and 'pause_enter' or 'pause_exit') end
        lastPaused=paused
    end
end
uevr.sdk.callbacks.on_pre_engine_tick(function()update(false)end)
uevr.sdk.callbacks.on_post_engine_tick(function()update(true)end)

end)(AC8Runtime.host('utility','AC8_VR-GlowLifecycle.lua'),AC8Runtime.json,AC8Runtime.log)
end


end
assert(not registry.sections['hud_policy'], 'Duplicate AC8 VR section: hud_policy')
registry.sections['hud_policy'] = function(R)
do
-- ac8 hud policy v50
(function(uevr,json,print)


local api=uevr.api
local old=rawget(_G,'AC8HudPolicyV50');if old then old.stopped=true end
local S={version=59,stopped=false,elapsed=0,nextScan=0,candidates={},roots={},nativeRootPaths={}}
rawset(_G,'AC8HudPolicyV50',S)
-- Reuse only read-only zero-argument UObject queries within THIS update.
-- Query caches start empty each update. Derived rectangles may survive only while current affine signatures match.
local frameReads,frameBounds,frameChains
local noValue={}
local readMethods={get_full_name=true,GetParent=true,get_outer=true,
    GetVisibility=true,GetRenderOpacity=true,GetActiveWidget=true,
    GetClipping=true,GetPaintSpaceGeometry=true,GetCachedGeometry=true}
local function call(o,k,...)
    if o==api then
        local ok,v=pcall(function(...)return o[k](o,...)end,...);if ok then return v end;return
    end
    if o==nil then return end
    if frameReads and readMethods[k] and select('#',...)==0 then
        local row=frameReads[o]
        local v=row and row[k]
        if v~=nil then
            if not AC8Runtime.isLiveObject(o)then return end
            S.frameReadHits=(S.frameReadHits or 0)+1
            if v~=noValue then return v end
            return
        end
        -- objectCall performs the lifetime check immediately before engine access.
        v=AC8Runtime.objectCall(o,k,...)
        if not row then row={};frameReads[o]=row end
        row[k]=v==nil and noValue or v
        return v
    end
    return AC8Runtime.objectCall(o,k,...)
end
local function field(o,k)local ok,v=pcall(function()return o[k]end);if ok then return v end end
local function full(o)return call(o,'get_full_name')end
local function xy(v)
    local x=field(v,'X') or field(v,'x');local y=field(v,'Y') or field(v,'y')
    if type(x)=='number' and type(y)=='number' and x==x and y==y and math.abs(x)<1e7 and math.abs(y)<1e7 then return {X=x,Y=y}end
end
local slate=call(call(api,'find_uobject','Class /Script/UMG.SlateBlueprintLibrary'),'get_class_default_object')
local function geometry(w)return call(w,'GetPaintSpaceGeometry') or call(w,'GetCachedGeometry')end
local function parent(w)
    local p=call(w,'GetParent');if p then return p end
    local outer=call(w,'get_outer');if (full(outer) or ''):match('^WidgetTree ')then return call(outer,'get_outer')end
end
local function chain(w,depth)
    depth=depth or 0;if depth>=32 then return nil end
    if not AC8Runtime.isLiveObject(w)then return nil end
    local cached=frameChains and frameChains[w]
    if cached~=nil then
        S.frameChainHits=(S.frameChainHits or 0)+1
        if cached==noValue or #cached+depth>32 then return nil end
        return cached
    end
    if frameChains then frameChains[w]=noValue end -- Also breaks cycles.
    local function fail()if frameChains then frameChains[w]=nil end;return nil end
    local n=full(w);if not n then return fail()end
    local vis=call(w,'GetVisibility');local opacity=call(w,'GetRenderOpacity')
    if type(vis)~='number'or vis==1 or vis==2 or type(opacity)~='number'or opacity<=0 then return fail()end
    local result={w}
    if not S.roots[n]then
        local p=parent(w);if not p then return fail()end
        local active=call(p,'GetActiveWidget');if active and full(active)~=n then return fail()end
        local tail=chain(p,depth+1);if not tail then return fail()end
        for _,o in ipairs(tail)do if full(o)==n then return fail()end;result[#result+1]=o end
    end
    if frameChains then frameChains[w]=result end
    return result
end
-- Cross-frame reuse only after re-reading the complete affine mapping used by
-- this rectangle. Never infer stability from a widget name, a delay, or a slot.
-- Geometry, local size, and visibility/clipping still come from this update.
local rectangleCache, frameRootStamp = {}, nil
local function resetRectangleCache()
    rectangleCache = {}; frameRootStamp = nil
    S.staticCacheResets = (S.staticCacheResets or 0) + 1
end
local function vectorNumbers(v)
    local x = field(v, 'X') or field(v, 'x')
    local y = field(v, 'Y') or field(v, 'y')
    if type(x) ~= 'number' or type(y) ~= 'number' or x ~= x or y ~= y or
       math.abs(x) == math.huge or math.abs(y) == math.huge then return end
    return x, y
end
local function sameNumbers(a, b)
    if not a or not b or #a ~= #b then return false end
    for i = 1, #a do if a[i] ~= b[i] then return false end end
    return true
end
local function rootStamp(g, size)
    local result = {size.X, size.Y}
    for _, p in ipairs({{X=0,Y=0}, {X=size.X,Y=0}, {X=0,Y=size.Y}}) do
        local x, y = vectorNumbers(call(slate, 'LocalToAbsolute', g, p))
        if x == nil then return end
        result[#result+1] = x; result[#result+1] = y
    end
    return result
end
local function calculateRawBounds(w, rootGeometry, rootSize, inset)
    local g = geometry(w)
    local size = xy(call(slate, 'GetLocalSize', g)); inset = inset or 0
    if not size or size.X <= 2*inset or size.Y <= 2*inset then return nil end
    local absolute, signature = {}, {size.X, size.Y, inset}
    local canCache = frameRootStamp ~= nil
    for _, p in ipairs({{X=inset,Y=inset}, {X=size.X-inset,Y=inset}, {X=inset,Y=size.Y-inset}}) do
        local value = call(slate, 'LocalToAbsolute', g, p)
        absolute[#absolute+1] = {value=value}
        local x, y = vectorNumbers(value)
        if x == nil then canCache = false
        else signature[#signature+1] = x; signature[#signature+1] = y end
    end
    local entries = rectangleCache[w]
    local cached = entries and entries[inset]
    if canCache and cached and sameNumbers(cached.root, frameRootStamp) and
       sameNumbers(cached.geometry, signature) then
        S.staticCacheHits = (S.staticCacheHits or 0) + 1
        if cached.bounds ~= noValue then return cached.bounds end
        return nil
    end
    S.staticCacheMisses = (S.staticCacheMisses or 0) + 1
    local function calculate()
        local points = {}
        for _, p in ipairs(absolute) do
            local value = xy(call(slate, 'AbsoluteToLocal', rootGeometry, p.value))
            if not value then return nil end
            points[#points+1] = value
        end
        if math.abs(points[1].Y-points[2].Y)>0.5 or math.abs(points[1].X-points[3].X)>0.5 then return nil end
        if not xy({X=points[2].X+points[3].X-points[1].X,Y=points[2].Y+points[3].Y-points[1].Y}) then return nil end
        return {math.min(points[1].X,points[2].X)/rootSize.X, math.min(points[1].Y,points[3].Y)/rootSize.Y,
            math.max(points[1].X,points[2].X)/rootSize.X, math.max(points[1].Y,points[3].Y)/rootSize.Y}
    end
    local result = calculate()
    if canCache then
        if not entries then entries = {}; rectangleCache[w] = entries end
        -- Store only numeric snapshots and derived bounds, not live geometry structs.
        entries[inset] = {root=frameRootStamp, geometry=signature, bounds=result or noValue}
    elseif entries then entries[inset] = nil end
    return result
end

local function rawBounds(w,g,size,inset)
    if not frameBounds then return calculateRawBounds(w,g,size,inset)end
    if not AC8Runtime.isLiveObject(w)then return end
    local row=frameBounds[w];inset=inset or 0
    if not row or row.geometry~=g or row.x~=size.X or row.y~=size.Y then
        row={geometry=g,x=size.X,y=size.Y,values={}};frameBounds[w]=row
    end
    local value=row.values[inset]
    if value~=nil then S.frameBoundsHits=(S.frameBoundsHits or 0)+1;if value~=noValue then return value end;return end
    value=calculateRawBounds(w,g,size,inset);row.values[inset]=value or noValue
    return value
end
local function intersection(a,b)
    if not a or not b then return nil end
    local r={math.max(a[1],b[1]),math.max(a[2],b[2]),math.min(a[3],b[3]),math.min(a[4],b[4])}
    if r[1]>=r[3] or r[2]>=r[4]then return nil end;return r
end
local function bounds(w,g,size,inset)
    local ancestors=chain(w);if not ancestors then return nil end
    local r=intersection(rawBounds(w,g,size,inset),{0,0,1,1});if not r then return nil end

    for _,a in ipairs(ancestors)do
        local c=call(a,'GetClipping') or call(a,'get_property','Clipping')
        if c==1 or c==2 or c==3 or c==4 then
            local clip=rawBounds(a,g,size,0)
            if clip then r=intersection(r,clip);if not r then return nil end end
            if c==2 then break end
        end
    end
    return r
end
local artworkBounds=bounds -- Separate only this region in the existing sampled geometry scope.
-- Temporary artwork trial: ONE PlaneIconStatPanel region reuses the existing
-- emissive policy path. No extra render pass or per-bar discovery/setters.
-- The separate text-stat panel is outside this region.
-- Twelve LOCAL widget pixels inset SHRINKS the excluded interior. The surrounding
-- twelve-pixel band gets ordinary authored glow; no special portrait border pass.
local function kind(n,w)
    local class,path=n:match('^(%S+) (.+)$');if not path then return nil end
    local leaf=path:match('%.([^%.]+)$')or''
    if class=='Image'and leaf=='PlaneIconBackGround'and path:find('.WBP_PlayerStateWidget_1.',1,true)then return 'playerstate-art-region',0,{0,0,1,0}end
    if class=='Image'and leaf=='Portrait'and path:find('.HUDMessageWidget.',1,true)then return 'portrait',12,{1,1,0,1}end
    if class=='Image'and leaf=='LayerTwoBackground'and path:find('.WBP_HUD_Message_000.',1,true)then return 'banner',0,{1,0,1,1}end
end
-- Resolve the two exact authored parents only during existing discovery.
-- The region deliberately includes both image siblings and all weapon/flare
-- bars. Keep source textures, tint, opacity, reload percentages and game writes.
local function candidate(w,n,category,inset,flags)
    if category=='playerstate-art-region'then
        local path=n:match('^Image (.+)%.PlaneIconBackGround$')
        local plane=call(w,'GetParent');local region=call(plane,'GetParent')
        if not path or full(plane)~='CanvasPanel '..path..'.PlaneCanvas' or
           full(region)~='CanvasPanel '..path..'.PlaneIconStatPanel'then return end
        local tree=call(w,'get_outer')
        if not tree or call(plane,'get_outer')~=tree or call(region,'get_outer')~=tree then return end
        w=region;n=full(region)
    end
    if chain(w)then return{widget=w,name=n,category=category,inset=inset,flags=flags}end
end
local function legacyDiscover()
    S.legacyScans=(S.legacyScans or 0)+1
    resetRectangleCache() -- Discovery bounds the cache and refreshes owner membership.
    S.candidates={};S.roots={};S.root=nil;frameChains={}
    local hud=call(call(api,'get_player_controller',0),'GetHUD');S.root=call(hud,'get_property','HudWidget');S.rootName=full(S.root)
    if not S.rootName then return end
    local roots=call(call(api,'find_uobject','Class /Script/UMG.UserWidget'),'get_objects_matching',false) or {}
    local prefixes,ownedRoots={},{}
    local function addRoot(w)
        local n=full(w) or '';local path=n:match('^%S+ (.+)$')
        if path and path:find('/Engine/Transient.',1,true) and not S.roots[n] then S.roots[n]=true;prefixes[#prefixes+1]=path;ownedRoots[#ownedRoots+1]=w end
    end

    for _,key in ipairs({'HudWidget','SubtitleWidget','HUDMessageWidget','NonGlowWidget','GlowWidgetPtr','NonGlowFrontWidgetPtr','NonGlowBackWidgetPtr'})do
        addRoot(call(hud,'get_property',key))
    end
    for _,w in ipairs(roots)do
        local n=full(w) or '';local path=n:match('^%S+ (.+)$')
        if path and path:find('/Engine/Transient.',1,true) and
            (call(w,'IsInViewport')==true or (S.elapsed<(S.nativeRootsUntil or 0) and S.nativeRootPaths[path]))then
            addRoot(w)
        end
    end
    local widgets,seen={},{}
    local complete=true
    for _,root in ipairs(ownedRoots)do
        -- A fresh walk supplies names from this exact call, not a cross-frame
        -- name cache. Avoid repeating get_full_name for every discovered child.
        local owned,names=AC8Runtime.ownedWidgets(root,true)
        if names and frameReads then
            for w,n in pairs(names)do
                if AC8Runtime.isLiveObject(w)then
                    local row=frameReads[w]or{};frameReads[w]=row;row.get_full_name=n
                end
            end
        end
        if not owned then complete=false;break end
        for _,w in ipairs(owned)do
            local n=full(w)
            if n and not seen[n]then seen[n]=true;widgets[#widgets+1]=w end
        end
    end
    if not complete then
        widgets=call(call(api,'find_uobject','Class /Script/UMG.Widget'),'get_objects_matching',false) or {}
        S.fallbackScans=(S.fallbackScans or 0)+1
    else S.ownedScans=(S.ownedScans or 0)+1 end
    for i,w in ipairs(widgets)do
        if i>32768 then break end
        local n=full(w) or '';local category,inset,flags=kind(n,w);local path=n:match('^%S+ (.+)$')
        if category and path then
            for _,prefix in ipairs(prefixes)do
                if path==prefix or path:sub(1,#prefix+1)==prefix..'.'then

                    local row=candidate(w,n,category,inset,flags);if row then S.candidates[#S.candidates+1]=row end
                    break
                end
            end
        end
    end
    local priority={portrait=1,['playerstate-art-region']=2,['plane-linework']=2,['missile-art']=3,['countermeasure-art']=3,banner=4,['radio-normal-glow']=5}
    table.sort(S.candidates,function(a,b)
        local pa,pb=priority[a.category] or 9,priority[b.category] or 9
        if pa~=pb then return pa<pb end;return a.name<b.name
    end)
    while #S.candidates>128 do table.remove(S.candidates)end
end
-- Only Image objects identify the policy rules, including the artwork anchor. Enumerate that
-- class and prove painted ownership by following reciprocal parent/tree links.
-- No global widget walk is needed when those links can be verified.
local imageClass, userClass
-- SDK short name avoids rebuilding every outer path for unrelated leaf images.
-- Missing/failed short-name API retains the old full-name route.
local function relevantImage(w)
    local leaf=call(w,'get_short_name')
    return type(leaf)~='string' or leaf=='Portrait' or leaf=='LayerTwoBackground' or leaf=='PlaneIconBackGround'
end
local function classObjects(which)
    local class
    if which == 'images' then class = imageClass else class = userClass end
    if not AC8Runtime.isLiveObject(class) then
        class = call(api, 'find_uobject', which == 'images' and 'Class /Script/UMG.Image' or 'Class /Script/UMG.UserWidget')
        if which == 'images' then imageClass = class else userClass = class end
    end
    return call(class, 'get_objects_matching', false)
end
local function sortedCandidates(rows)
    table.sort(rows,function(a,b)
        local pa,pb=a.category=='portrait'and 1 or a.category=='playerstate-art-region'and 2 or 4,b.category=='portrait'and 1 or b.category=='playerstate-art-region'and 2 or 4
        if pa~=pb then return pa<pb end
        return a.name<b.name
    end)
    while #rows>128 do table.remove(rows)end
    return rows
end
local function viewportRoots(users,nativeWarm,addRoot)
    for _,w in ipairs(users)do
        -- The old walk fetched every full name before testing viewport state.
        local inViewport=call(w,'IsInViewport')==true
        if inViewport or nativeWarm then
            local n=full(w)or'';local path=n:match('^%S+ (.+)$')
            if path and path:find('/Engine/Transient.',1,true)and(inViewport or S.nativeRootPaths[path])then addRoot(w)end
        end
    end
end
local function discover()
    local hud=call(call(api,'get_player_controller',0),'GetHUD')
    local root=call(hud,'get_property','HudWidget')
    local rootName=full(root)
    if not rootName then
        S.root=nil;S.rootName=nil;S.roots={};S.candidates={};resetRectangleCache()
        return
    end
    local users,images=classObjects('users'),classObjects('images')
    S.discoveryUsers=type(users)=='table'and #users or 0;S.discoveryImages=type(images)=='table'and #images or 0
    if not users or not images or #images==0 then
        S.fastUnavailable=(S.fastUnavailable or 0)+1
        return legacyDiscover()
    end
    S.root=root;S.rootName=rootName;S.roots={};frameChains={}
    local prefixes={}
    local function addRoot(w)
        local n=full(w)or'';local path=n:match('^%S+ (.+)$')
        if path and path:find('/Engine/Transient.',1,true)and not S.roots[n]then
            S.roots[n]=true;prefixes[#prefixes+1]=path
        end
    end
    for _,key in ipairs({'HudWidget','SubtitleWidget','HUDMessageWidget','NonGlowWidget','GlowWidgetPtr','NonGlowFrontWidgetPtr','NonGlowBackWidgetPtr'})do addRoot(call(hud,'get_property',key))end
    local nativeWarm=S.elapsed<(S.nativeRootsUntil or 0)
    viewportRoots(users,nativeWarm,addRoot)
    local membership,visiting={},{}
    local function belongs(w,depth)
        depth=depth or 0
        if depth>=32 or not AC8Runtime.isLiveObject(w)then return false end
        if visiting[w]then return false end
        if membership[w]~=nil then
            if membership[w]==noValue then return nil end
            return membership[w]
        end
        local n=full(w)
        if not n then return false end
        if S.roots[n]then membership[w]=true;return true end
        visiting[w]=true
        local function done(value)
            visiting[w]=nil;membership[w]=value==nil and noValue or value
            return value
        end
        local p=call(w,'GetParent')
        if p then
            local index=call(p,'GetChildIndex',w)
            if type(index)=='number'and index==math.floor(index)then
                if index<0 then return done(false)end
                if index>4095 then return done(nil)end
                if call(p,'GetChildAt',index)~=w then return done(nil)end
            else
                local count=call(p,'GetChildrenCount')
                if type(count)~='number'or count~=math.floor(count)or count<0 or count>256 then return done(nil)end
                local found=false
                for i=0,count-1 do if call(p,'GetChildAt',i)==w then found=true;break end end
                if not found then return done(false)end
            end
            return done(belongs(p,depth+1))
        end
        local tree=call(w,'get_outer')
        if not(full(tree)or''):match('^WidgetTree ')then return done(false)end
        local first=call(tree,'get_property','RootWidget')
        if not first then return done(nil)end
        if first~=w then return done(false)end -- Reject detached/orphaned children.
        local owner=call(tree,'get_outer')
        if not owner or call(owner,'get_property','WidgetTree')~=tree then return done(nil)end
        return done(belongs(owner,depth+1))
    end
    local candidates,seen={},{}
    for i,w in ipairs(images)do
        if i>32768 then return legacyDiscover()end
        local n=relevantImage(w)and(full(w)or'')or'';local category,inset,flags=kind(n,w)
        if category and not seen[n]then
            local path=n:match('^%S+ (.+)$')
            if path then
                for _,prefix in ipairs(prefixes)do
                    if path==prefix or path:sub(1,#prefix+1)==prefix..'.'then
                        local owned=belongs(w)
                        if owned~=true then
                            S.fastAmbiguous=(S.fastAmbiguous or 0)+1
                            return legacyDiscover()
                        end
                        if owned then
                            local row=candidate(w,n,category,inset,flags)
                            if row then candidates[#candidates+1]=row;seen[n]=true end
                        end
                        break
                    end
                end
            end
        end
    end
    S.candidates=sortedCandidates(candidates)
    S.imageScans=(S.imageScans or 0)+1
    -- Keep validated numeric geometry for surviving candidates/clip ancestors.
    -- Per-frame affine checks still invalidate any movement/scale/resize.
    local keep={}
    for _,candidate in ipairs(S.candidates)do
        for _,ancestor in ipairs(chain(candidate.widget)or{})do keep[ancestor]=true end
    end
    for w in pairs(rectangleCache)do if not keep[w]then rectangleCache[w]=nil end end
end

-- Narrow native source roster; retain old discovery when the helper/API is
-- absent, incomplete or asynchronously delivered. No stale roster is accepted.
local fullDiscover=discover
local sourceRequest,sourceReply=0,nil
local function sourceImages()
    sourceRequest=sourceRequest%999999999+1;sourceReply=nil
    local wanted=tostring(sourceRequest)
    pcall(function()api:dispatch_custom_event('AC8_GlowDiscoveryRequest',wanted)end)
    local reply=sourceReply;sourceReply=nil
    if not reply or reply.request~=wanted or reply.status~='ok'then
        S.sourceFallbacks=(S.sourceFallbacks or 0)+1;return
    end
    local result={}
    for _,n in ipairs(reply.names)do
        local w=call(api,'find_uobject',n)
        if not w or full(w)~=n then S.sourceFallbacks=(S.sourceFallbacks or 0)+1;return end
        result[#result+1]=w
    end
    S.nativeSourceQueries=(S.nativeSourceQueries or 0)+1
    S.relevantSourceCount=#result
    return result
end
-- Only policy-bearing images can require an additional viewport root. Walk
-- those few reciprocal owner paths, not every UserWidget in the process.
-- Hidden candidates, dynamic creation, native roots, detach/reparent and
-- same-name replacement still pass the original ownership/candidate checks.
local function paintedRoot(w,addRoot,nativeWarm,depth,seen,isOwner)
    depth=depth or 0;seen=seen or{}
    if depth>=32 or not AC8Runtime.isLiveObject(w)or seen[w]then return end
    seen[w]=true
    local n=full(w)or'';local path=n:match('^%S+ (.+)$')
    if S.roots[n]then return true end
    if isOwner and path and path:find('/Engine/Transient.',1,true)and
       (call(w,'IsInViewport')==true or(nativeWarm and S.nativeRootPaths[path]))then
        addRoot(w);return true
    end
    local p=call(w,'GetParent')
    if p then return paintedRoot(p,addRoot,nativeWarm,depth+1,seen,false)end
    local tree=call(w,'get_outer')
    if not(full(tree)or''):match('^WidgetTree ')then return end
    local owner=call(tree,'get_outer')
    if owner and call(owner,'get_property','WidgetTree')==tree then
        return paintedRoot(owner,addRoot,nativeWarm,depth+1,seen,true)
    end
end
local function sourceDiscover()
    local hud=call(call(api,'get_player_controller',0),'GetHUD')
    local root=call(hud,'get_property','HudWidget');local rootName=full(root)
    if not rootName then
        S.root=nil;S.rootName=nil;S.roots={};S.candidates={};resetRectangleCache();return
    end
    local images=sourceImages()
    if not images then return fullDiscover()end
    S.root=root;S.rootName=rootName;S.roots={};frameChains={}
    local prefixes={}
    local function addRoot(w)
        local n=full(w)or'';local path=n:match('^%S+ (.+)$')
        if path and path:find('/Engine/Transient.',1,true)and not S.roots[n]then
            S.roots[n]=true;prefixes[#prefixes+1]=path
        end
    end
    for _,key in ipairs({'HudWidget','SubtitleWidget','HUDMessageWidget','NonGlowWidget','GlowWidgetPtr','NonGlowFrontWidgetPtr','NonGlowBackWidgetPtr'})do addRoot(call(hud,'get_property',key))end
    local nativeWarm=S.elapsed<(S.nativeRootsUntil or 0)
    for _,w in ipairs(images)do paintedRoot(w,addRoot,nativeWarm)end
    local membership,visiting={},{}
    local function belongs(w,depth)
        depth=depth or 0
        if depth>=32 or not AC8Runtime.isLiveObject(w)then return false end
        if visiting[w]then return false end
        if membership[w]~=nil then
            if membership[w]==noValue then return nil end
            return membership[w]
        end
        local n=full(w)
        if not n then return false end
        if S.roots[n]then membership[w]=true;return true end
        visiting[w]=true
        local function done(value)
            visiting[w]=nil;membership[w]=value==nil and noValue or value
            return value
        end
        local p=call(w,'GetParent')
        if p then
            local index=call(p,'GetChildIndex',w)
            if type(index)=='number'and index==math.floor(index)then
                if index<0 then return done(false)end
                if index>4095 then return done(nil)end
                if call(p,'GetChildAt',index)~=w then return done(nil)end
            else
                local count=call(p,'GetChildrenCount')
                if type(count)~='number'or count~=math.floor(count)or count<0 or count>256 then return done(nil)end
                local found=false
                for i=0,count-1 do if call(p,'GetChildAt',i)==w then found=true;break end end
                if not found then return done(false)end
            end
            return done(belongs(p,depth+1))
        end
        local tree=call(w,'get_outer')
        if not(full(tree)or''):match('^WidgetTree ')then return done(false)end
        local first=call(tree,'get_property','RootWidget')
        if not first then return done(nil)end
        if first~=w then return done(false)end -- Reject detached/orphaned children.
        local owner=call(tree,'get_outer')
        if not owner or call(owner,'get_property','WidgetTree')~=tree then return done(nil)end
        return done(belongs(owner,depth+1))
    end
    local candidates,seen={},{}
    for _,w in ipairs(images)do
        local n=full(w)or'';local category,inset,flags=kind(n,w)
        if category and not seen[n]then
            local path=n:match('^%S+ (.+)$')
            if path then
                for _,prefix in ipairs(prefixes)do
                    if path==prefix or path:sub(1,#prefix+1)==prefix..'.'then
                        if belongs(w)~=true then
                            S.fastAmbiguous=(S.fastAmbiguous or 0)+1;return legacyDiscover()
                        end
                        local row=candidate(w,n,category,inset,flags)
                        if row then candidates[#candidates+1]=row;seen[n]=true end
                        break
                    end
                end
            end
        end
    end
    S.candidates=sortedCandidates(candidates);S.imageScans=(S.imageScans or 0)+1
    local keep={}
    for _,c in ipairs(S.candidates)do for _,ancestor in ipairs(chain(c.widget)or{})do keep[ancestor]=true end end
    for w in pairs(rectangleCache)do if not keep[w]then rectangleCache[w]=nil end end
end
-- Ordinary sampling: parent discovery includes synchronous native roster work.
-- Native event callbacks also retain their existing CPU wrapper ownership.
discover=sourceDiscover

-- Compare geometry every frame and reuse the serialized packet when unchanged.
-- Continue sending it every frame: the native 250ms stale-policy safeguard uses
-- wall time, so a paused/dilated game clock must never delay its heartbeat.
local function publishPolicy(rectangles)
    local changed=not S.policyValues or S.policyCount~=#rectangles
    if not changed then
        local index=1
        for _,r in ipairs(rectangles)do
            for i=1,4 do
                if S.policyValues[index+i-1]~=r.bounds[i] or S.policyValues[index+i+3]~=r.flags[i]then changed=true end
            end
            index=index+8
        end
    end
    if changed then
        local values={tostring(#rectangles)};S.policyValues={};S.policyCount=#rectangles
        for _,r in ipairs(rectangles)do
            for _,v in ipairs(r.bounds)do values[#values+1]=string.format('%.9g',v);S.policyValues[#S.policyValues+1]=v end
            for _,v in ipairs(r.flags)do values[#values+1]=tostring(v);S.policyValues[#S.policyValues+1]=v end
        end
        S.policyPacket=table.concat(values,'|');S.policyBuilds=(S.policyBuilds or 0)+1
    end
    local ok=pcall(function()api:dispatch_custom_event('AC8_HUD_PolicyV50',S.policyPacket)end)
    if ok then S.policySends=(S.policySends or 0)+1 end
end

local function buildPolicy()
    local rectangles={}
    local function add(r,flags)
        if not r then return end

        for i=#rectangles,1,-1 do
            local a=rectangles[i];local same=true
            for j=1,4 do if flags[j]~=a.flags[j]then same=false end end
            if same then
                if r[1]>=a.bounds[1] and r[2]>=a.bounds[2] and r[3]<=a.bounds[3] and r[4]<=a.bounds[4]then return end
                if a.bounds[1]>=r[1] and a.bounds[2]>=r[2] and a.bounds[3]<=r[3] and a.bounds[4]<=r[4]then table.remove(rectangles,i)end
            end
        end
        rectangles[#rectangles+1]={bounds=r,flags=flags}
    end
    if S.root and full(S.root)==S.rootName then
        local g=geometry(S.root);local size=xy(call(slate,'GetLocalSize',g))
        frameRootStamp=(size and size.X>0 and size.Y>0 and #S.candidates>0)and rootStamp(g,size)or nil
        if size and size.X>0 and size.Y>0 then
            for _,c in ipairs(S.candidates)do
                if full(c.widget)==c.name then
                    local usable=true
                    if c.category=='portrait' then
                        local brush=call(c.widget,'GetBrush') or call(c.widget,'get_property','Brush')
                        local resource=field(brush,'ResourceObject')
                        local color=call(c.widget,'GetColorAndOpacity') or call(c.widget,'get_property','ColorAndOpacity')
                        local alpha=field(color,'A')
                        usable=resource~=nil and (type(alpha)~='number' or alpha>0)
                    end
                    local r
                    if usable then
                        if c.category=='playerstate-art-region'then r=artworkBounds(c.widget,g,size,c.inset)
                        else r=bounds(c.widget,g,size,c.inset)end
                    end
                    if r and c.category=='playerstate-art-region'and
                       (r[3]-r[1])*(r[4]-r[2])>0.25 then
                        r=nil;S.artRegionRejected=(S.artRegionRejected or 0)+1
                    end
                    if r then
                        add(r,c.flags,c.name,c.category)

                    end
                end
            end
        end
    end
    
    if #rectangles>48 then rectangles={};S.overflow=true else S.overflow=false end
    return rectangles
end








local function update(delta)
    if S.stopped then return end
    if AC8Runtime.glowGameplayAllowed==false then
        if not S.gameplaySuspended then
            publishPolicy({});S.gameplaySuspended=true;resetRectangleCache()
        end
        return
    end
    if S.gameplaySuspended then S.gameplaySuspended=false;S.nextScan=0;resetRectangleCache() end
    local epoch=R.context and R.context.epoch
    if epoch~=S.ownerEpoch then S.ownerEpoch=epoch;S.nextScan=0;resetRectangleCache() end
    frameReads={};frameBounds={};frameChains={}
    S.elapsed=S.elapsed+math.max(0,math.min(tonumber(delta) or 0.016,1))
    if S.elapsed>=S.nextScan then discover();S.nextScan=S.elapsed+0.5 end
    local rectangles=buildPolicy()
    publishPolicy(rectangles)
    frameReads=nil;frameBounds=nil;frameChains=nil;frameRootStamp=nil
end
uevr.sdk.callbacks.on_post_engine_tick(function(_,delta)update(delta)end)
uevr.sdk.callbacks.on_lua_event(function(name,data)
    if S.stopped or type(data)~='string'then return end
    if name=='AC8_GlowDiscoverySources'then
        if #data>1048576 then return end
        local request,status,tail=data:match('^(%d+)|(%a+)(.*)$')
        if request~=tostring(sourceRequest)or(status~='ok'and status~='fallback')then return end
        local names={}
        for n in (tail or''):gmatch('|([^|]+)')do
            if #names>=256 or not n:match('^Image ')then return end
            names[#names+1]=n
        end
        sourceReply={request=request,status=status,names=names};return
    end
    if name~='AC8_UI_RegisteredRoots'then return end
    S.nativeRootPaths={};S.nativeRootsUntil=S.elapsed+1.25;S.nextScan=0
    for path in data:gmatch('[^|]+')do S.nativeRootPaths[path]=true end
end)

end)(AC8Runtime.host('utility','AC8_VR-HUDPolicyV50.lua'),AC8Runtime.json,AC8Runtime.log)
end


end
-- Standalone profile: authored HUD brightness/effects belong to HUD Layout
-- and the game. No cross-profile brightness normalization, capture/restore
-- adapter, periodic brightness discovery or late opacity override is installed.

if registry.start then registry.start()end


