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
-- Stable settings owned by this file. UI edits still use their existing
-- in-memory values; external preference edits reload on script/game restart.
-- Explicit user saves use verified preference storage.
-- User preferences only.
local hudProfiles

local preferenceIO=json
-- File-local group settings; all users are inside this HUD module.
local simpleHUD={values={},keys={'fpm','portrait','radio','wingman','orders','mission','score','target','time'}}
function simpleHUD.normalize(input)
    local out={}
    for _,key in ipairs(simpleHUD.keys)do
        local p=type(input)=='table'and input[key]or nil
        local function axis(k)
            local v=type(p)=='table'and tonumber(p[k])or nil
            return v and v==v and v>=-4000 and v<=4000 and v or 0
        end
        out[key]={x=axis('x'),y=axis('y')}
        if key=='mission'or key=='wingman'then
            local scale=type(p)=='table'and tonumber(p.scale)or nil
            out[key].scale=scale and scale==scale and scale>=.1 and scale<=3 and scale or 1
        end
    end
    return out
end
simpleHUD.values=simpleHUD.normalize()
function simpleHUD.scale(key)
    local p=simpleHUD.values[key]
    return p and p.scale or 1
end
-- Neutral HUD defaults. Legacy coordinate layouts remain saved but are not applied.
-- Newly saved layouts carry coordinateRevision=2 and restore normally.
function simpleHUD.baselineLayout()
    local layout={coordinateRevision=2,weaponCalibration={saamScale=1,bombFov=12.5,bombY=0},groups={},components={},simpleGroups=simpleHUD.normalize(),
        canvas={enabled=false,x=0,y=0,scale=1,extraWidth=0,extraHeight=0,inheritClip=false}}
    for _,key in ipairs({'ladder','radar','info'})do layout.groups[key]={enabled=true,x=0,y=0,scale=1}end
    return layout
end
simpleHUD.weapon={saamScale=1,bombFov=12.5,bombY=0}
function simpleHUD.weaponValues(v)
    v=type(v)=='table'and v or{}
    local function number(key,default,low,high)local n=tonumber(v[key]);return n and n==n and n>=low and n<=high and n or default end
    return{saamScale=number('saamScale',1,.25,2),bombFov=number('bombFov',12.5,-40,40),bombY=number('bombY',0,-4000,4000)}
end
function simpleHUD.targetCalibration(v)
    if type(v)~='table'or type(v.scope)~='string'then return nil end
    for _,key in ipairs({'x','y','sx','sy'})do
        if type(v[key])~='number'or v[key]~=v[key]then return nil end
    end
    if math.abs(v.x)>4000 or math.abs(v.y)>4000 or v.sx<.1 or v.sx>3 or v.sy<.1 or v.sy>3 then return nil end
    return{x=v.x,y=v.y,sx=v.sx,sy=v.sy,scope=v.scope}
end
function simpleHUD.offset(keys)
    local x,y=0,0
    for _,key in ipairs(keys)do local p=simpleHUD.values[key];if p then x=x+p.x;y=y+p.y end end
    return x,y
end
local function invokeInventory(o,k,...)if AC8Runtime.inventory then return AC8Runtime.inventory.call(o,k,...)end;return o[k](o,...)end
local function simpleCall(o,k,...)
    local R=rawget(_G,'AC8Runtime')
    if not o or(o~=uevr.api and(not R or not R.isLiveObject(o)))then return end
    local ok,v=pcall(invokeInventory,o,k,...);if ok then return v end
end
local function simpleXY(v)
    local ok,x,y=pcall(function()return v.X,v.Y end)
    if ok and type(x)=='number'and type(y)=='number'and x==x and y==y then return{X=x,Y=y}end
end
local function simpleGeometry(o)
    return simpleCall(o,'GetPaintSpaceGeometry')or simpleCall(o,'GetCachedGeometry')
end
local function simpleSlate()
    local R=rawget(_G,'AC8Runtime')
    if not R or not R.isLiveObject(simpleHUD.slate)then
        simpleHUD.slate=simpleCall(simpleCall(uevr.api,'find_uobject','Class /Script/UMG.SlateBlueprintLibrary'),'get_class_default_object')
    end
    return simpleHUD.slate
end
function simpleHUD.absoluteOffset(keys)
    local x,y=simpleHUD.offset(keys)
    if x==0 and y==0 then return{X=0,Y=0}end
    local R=rawget(_G,'AC8Runtime');local root=R and R.context.root
    local slate=simpleSlate();local g=simpleGeometry(root)
    local a=simpleXY(simpleCall(slate,'LocalToAbsolute',g,{X=0,Y=0}))
    local b=simpleXY(simpleCall(slate,'LocalToAbsolute',g,{X=x,Y=y}))
    if a and b then return{X=b.X-a.X,Y=b.Y-a.Y}end
end
function simpleHUD.parentOffset(parent,keys)
    local x,y=simpleHUD.offset(keys)
    if x==0 and y==0 then return{X=0,Y=0}end
    local R=rawget(_G,'AC8Runtime');local root=R and R.context.root
    local slate=simpleSlate();local rg=simpleGeometry(root);local pg=simpleGeometry(parent)
    local a=simpleXY(simpleCall(slate,'LocalToAbsolute',rg,{X=0,Y=0}))
    local b=simpleXY(simpleCall(slate,'LocalToAbsolute',rg,{X=x,Y=y}))
    local pa=a and simpleXY(simpleCall(slate,'AbsoluteToLocal',pg,a))
    local pb=b and simpleXY(simpleCall(slate,'AbsoluteToLocal',pg,b))
    if pa and pb then return{X=pb.X-pa.X,Y=pb.Y-pa.Y}end
end

-- Render translations are in each widget's own layout units. Calibrate their
-- response in HUD-root units; GetParent geometry alone omits layout scaling.
simpleHUD.navigationBases=setmetatable({},{__mode='k'})
-- Retired combined navigation mover. Individual controls remain active.
function simpleHUD.navigationScale()return 1 end
function simpleHUD.navigationActive()return false end
local function rootGeometry()
    local R=rawget(_G,'AC8Runtime');local root=R and R.context.root
    local slate=simpleSlate();local g=simpleGeometry(root)
    local size=simpleXY(simpleCall(slate,'GetLocalSize',g));return slate,g,size,root
end
local function rootPoint(slate,rootGeometry,geometry,point)
    local a=simpleXY(simpleCall(slate,'LocalToAbsolute',geometry,point))
    return a and simpleXY(simpleCall(slate,'AbsoluteToLocal',rootGeometry,a))
end
local function inverseVector(b,x,y)
    local d=b.xx*b.yy-b.xy*b.yx
    if math.abs(d)<1e-9 then return end
    return(x*b.yy-y*b.xy)/d,(y*b.xx-x*b.yx)/d
end
local function paintBasis(o)
    local slate,rg,size,root=rootGeometry();local g=simpleGeometry(o)
    if not size or size.X<=0 or size.Y<=0 then return end
    local origin=rootPoint(slate,rg,g,{X=0,Y=0})
    local x=rootPoint(slate,rg,g,{X=1,Y=0});local y=rootPoint(slate,rg,g,{X=0,Y=1})
    if not origin or not x or not y then return end
    return origin,{xx=x.X-origin.X,yx=x.Y-origin.Y,xy=y.X-origin.X,yy=y.Y-origin.Y},size,root,g,slate
end
local function inheritedScale()return 1 end
local function renderMap(o,inherited)
    local slate,rg,size,root=rootGeometry();local cached=simpleHUD.navigationBases[o]
    -- Root size can remain fixed while a parent is re-scaled or re-laid out.
    -- Validate the current parent basis before reusing a local movement map.
    local parent=simpleCall(o,'GetParent');local pg=simpleGeometry(parent)
    local a=rootPoint(slate,rg,pg,{X=0,Y=0})
    local px=rootPoint(slate,rg,pg,{X=1,Y=0});local py=rootPoint(slate,rg,pg,{X=0,Y=1})
    local ps=simpleXY(simpleCall(slate,'GetLocalSize',pg))
    local own=simpleXY(simpleCall(slate,'GetLocalSize',simpleGeometry(o)))
    local fingerprint=a and px and py and ps and own and{a.X,a.Y,px.X-a.X,px.Y-a.Y,py.X-a.X,py.Y-a.Y,ps.X,ps.Y,own.X,own.Y}
    local same=fingerprint and cached and cached.parent==parent and cached.fingerprint
    if same then for i,v in ipairs(fingerprint)do if math.abs(v-cached.fingerprint[i])>.0001 then same=false;break end end end
    if same and cached.root==root and size and cached.width==size.X and cached.height==size.Y then return cached end
    local origin,paint,dimensions,owner,g=paintBasis(o);if not origin then return end
    local rt=simpleCall(o,'get_property','RenderTransform');local ok,t,scale,angle,shear=pcall(function()
        return simpleXY(rt.Translation),simpleXY(rt.Scale),tonumber(rt.Angle)or 0,simpleXY(rt.Shear)
    end)
    if not ok or not t or not scale or math.abs(scale.X)<1e-6 or math.abs(scale.Y)<1e-6 then return end
    -- Native group roots have zero render rotation/shear; rotated parents are
    -- supported by the full measured basis. Forward unchanged otherwise.
    if math.abs(angle)>1e-6 or shear and(math.abs(shear.X)>1e-6 or math.abs(shear.Y)>1e-6)then return end
    local outer=inherited and inheritedScale()or 1
    local b={xx=paint.xx/scale.X/outer,yx=paint.yx/scale.X/outer,
        xy=paint.xy/scale.Y/outer,yy=paint.yy/scale.Y/outer,sx=scale.X,sy=scale.Y,
        root=owner,width=dimensions.X,height=dimensions.Y}
    if not inverseVector(b,1,0)then return end
    local localSize=simpleXY(simpleCall(slate,'GetLocalSize',g));if not localSize then return end
    local pivot=simpleXY(simpleCall(o,'GetRenderTransformPivot'))or{X=0,Y=0}
    b.px=pivot.X*localSize.X;b.py=pivot.Y*localSize.Y
    b.cx=localSize.X/2;b.cy=localSize.Y/2
    b.lx=origin.X-b.xx*(t.X+b.px-scale.X*b.px)-b.xy*(t.Y+b.py-scale.Y*b.py)
    b.ly=origin.Y-b.yx*(t.X+b.px-scale.X*b.px)-b.yy*(t.Y+b.py-scale.Y*b.py)
    b.parent=parent;b.fingerprint=fingerprint
    simpleHUD.navigationBases[o]=b;return b
end
function simpleHUD.renderVector(o,x,y)
    if x==0 and y==0 then return 0,0 end
    local b=renderMap(o,false);if not b then return end
    return inverseVector(b,x,y)
end
function simpleHUD.renderOffset(o,keys,inherited)
    local x,y=simpleHUD.offset(keys);if x==0 and y==0 then return{X=0,Y=0}end
    local b=renderMap(o,inherited);if not b then return end
    local dx,dy=inverseVector(b,x,y);return dx and{X=dx,Y=dy}
end

function simpleHUD.prepareLadder(o)
    if AC8Runtime.isLiveObject(o)then simpleHUD.ladder=o end
end
-- 1920,1080 is the centre of the authored 3840x2160 HUD, not every live
-- canvas. A maximized 2560x1338 window has a 4132.735x2160 logical canvas:
-- retaining 1920 places the ladder/FPM about 91 physical pixels left.
-- Use the parent's logical size only. Retainer paint positions can lag our
-- writes and must never feed back into an alignment correction. Manual offsets
-- remain additive, and the existing FPM slot/pivot calculation follows this pose.
-- Share the bounded size read across application/readback in the same HUD tick.
simpleHUD.ladderSizes=setmetatable({},{__mode='k'})
function simpleHUD.ladderPosition(o,x,y)
    local R=rawget(_G,'AC8Runtime');local cached=simpleHUD.ladderSizes[o]
    if not R or not R.isLiveObject(o)then return end
    if not cached or cached.tick~=R.tick or cached.root~=R.context.root then
        local parent=simpleCall(o,'GetParent')
        local size=simpleXY(simpleCall(simpleSlate(),'GetLocalSize',simpleGeometry(parent)))
        if not size or size.X<=0 or size.Y<=0 or size.X>=math.huge or size.Y>=math.huge then return end
        cached={tick=R.tick,root=R.context.root,x=size.X/2-1920,y=size.Y/2-1080}
        simpleHUD.ladderSizes[o]=cached
    end
    return x+cached.x,y+cached.y
end
local function fpmCanvasLayout(o,parentSize)
    local slot=simpleCall(o,'get_property','Slot')
    if not(simpleCall(slot,'get_full_name')or''):match('^CanvasPanelSlot ')then return end
    local offsets=simpleCall(slot,'GetOffsets');local anchors=simpleCall(slot,'GetAnchors')
    local alignment=simpleXY(simpleCall(slot,'GetAlignment'))
    local ok,left,top,right,bottom,minimum,maximum=pcall(function()
        return offsets.Left,offsets.Top,offsets.Right,offsets.Bottom,simpleXY(anchors.Minimum),simpleXY(anchors.Maximum)
    end)
    if not ok or not minimum or not maximum or not alignment then return end
    local function finite(v)return type(v)=='number'and v==v and math.abs(v)<math.huge end
    if not finite(left)or not finite(top)or not finite(right)or not finite(bottom)then return end
    local stretchX=minimum.X~=maximum.X;local stretchY=minimum.Y~=maximum.Y
    local width=stretchX and(parentSize.X*(maximum.X-minimum.X)-left-right)or right
    local height=stretchY and(parentSize.Y*(maximum.Y-minimum.Y)-top-bottom)or bottom
    if simpleCall(slot,'GetAutoSize')==true then
        local desired=simpleXY(simpleCall(o,'GetDesiredSize'));if not desired then return end
        if not stretchX then width=desired.X end;if not stretchY then height=desired.Y end
    end
    if width<=0 or height<=0 then return end
    local pivot=simpleXY(simpleCall(o,'get_property','RenderTransformPivot'))or simpleXY(simpleCall(o,'GetRenderTransformPivot'))or{X=0,Y=0}
    return{x=parentSize.X*minimum.X+left-(stretchX and 0 or alignment.X*width),
        y=parentSize.Y*minimum.Y+top-(stretchY and 0 or alignment.Y*height),w=width,h=height,px=pivot.X*width,py=pivot.Y*height}
end
-- Gun/FPM reset positions were confirmed on the authored 3840x2160 canvas.
-- Keep that exact pose at 1080p (and any DPI scale with the same logical size).
-- Elsewhere, move the baseline by the parent-centre change, less the movement
-- already supplied by this widget's slot anchors, size and render pivot/scale.
-- Adding half the viewport difference alone would double-move centred slots.
-- Read current slot data, never the widget's stale painted position. These
-- bounded reads run only at the existing reset/initialization opportunities.
function simpleHUD.resetCenter(o,x,y,wantedScale)
    local parent=simpleCall(o,'GetParent')
    local size=simpleXY(simpleCall(simpleSlate(),'GetLocalSize',simpleGeometry(parent)))
    if not size or size.X<=0 or size.Y<=0 or size.X>=math.huge or size.Y>=math.huge then return end
    if size.X==3840 and size.Y==2160 then return x,y end
    local current=fpmCanvasLayout(o,size)
    local reference=fpmCanvasLayout(o,{X=3840,Y=2160})
    local rt=simpleCall(o,'get_property','RenderTransform')
    local ok,scale,angle,shear=pcall(function()
        return simpleXY(wantedScale or rt.Scale),tonumber(rt.Angle)or 0,simpleXY(rt.Shear)
    end)
    if not current or not reference or not ok or not scale or math.abs(angle)>1e-6 or shear and(math.abs(shear.X)>1e-6 or math.abs(shear.Y)>1e-6)then return end
    local dx=current.x+current.px+scale.X*(current.w/2-current.px)
        -(reference.x+reference.px+scale.X*(reference.w/2-reference.px))
    local dy=current.y+current.py+scale.Y*(current.h/2-current.py)
        -(reference.y+reference.py+scale.Y*(reference.h/2-reference.py))
    local tx,ty=x+(size.X-3840)/2-dx,y+(size.Y-2160)/2-dy
    if tx==tx and ty==ty and math.abs(tx)<math.huge and math.abs(ty)<math.huge then return tx,ty end
end
function simpleHUD.fpmPose(o,x,y,scale)
    local ladder=simpleHUD.ladder
    if not ladder or not AC8Runtime.isLiveObject(ladder)then return x,y,scale,scale,false end
    local parent=simpleCall(ladder,'GetParent');local other=simpleCall(o,'GetParent')
    if not parent or not other or simpleCall(parent,'get_full_name')~=simpleCall(other,'get_full_name')then return x,y,scale,scale,false end
    local size=simpleXY(simpleCall(simpleSlate(),'GetLocalSize',simpleGeometry(parent)))
    if not size or size.X<=0 or size.Y<=0 then return x,y,scale,scale,false end
    local a=fpmCanvasLayout(ladder,size);local b=fpmCanvasLayout(o,size)
    local rt=simpleCall(ladder,'get_property','RenderTransform')
    local ok,t,s,angle,shear=pcall(function()return simpleXY(rt.Translation),simpleXY(rt.Scale),tonumber(rt.Angle)or 0,simpleXY(rt.Shear)end)
    if not a or not b or not ok or not t or not s or s.X<=0 or s.Y<=0 or math.abs(angle)>1e-6 or shear and(math.abs(shear.X)>1e-6 or math.abs(shear.Y)>1e-6)then return x,y,scale,scale,false end
    -- Both native canvas roots share a parent. Use current slot data and pivot
    -- directly; painted geometry can still describe the pre-write frame.
    local cx=a.x+t.X+a.px+s.X*(a.w/2-a.px)
    local cy=a.y+t.Y+a.py+s.Y*(a.h/2-a.py)
    return cx-b.x-b.px-s.X*(b.w/2-b.px),cy-b.y-b.py-s.Y*(b.h/2-b.py),s.X,s.Y,true
end
function simpleHUD.navigationPoint(_,point)return{X=point.X,Y=point.Y}end
function simpleHUD.navigationPose(_,x,y,sx,sy)return x,y,sx,sy end
function simpleHUD.navigationUngroupPoint(_,point)return point end

-- End simple HUD helpers.

local preferences,preferenceLoaded={},{}
local function preference(loader,name)
 if not preferenceLoaded[name]then
  preferenceLoaded[name]=true
  preferences[name]=loader.load_file(name)
 end
 return preferences[name]
end
assert(not registry.sections['inherited_hud'], 'Duplicate AC8 VR section: inherited_hud')
registry.sections['inherited_hud'] = function(R)

AC8Runtime.inheritedSetup[#AC8Runtime.inheritedSetup+1]=function()

do
-- Flat2VR Modding Community and Discord: inherited hud_inspect_v129. Discover flight canvases; preserve warning visibility, instrument anchors, radar placement, rigid cockpit camera and saved effect controls. Use reflected setters to notify Slate. Authored constants and calibration bounds are retained.
(function(uevr,json,print)


local CANVAS_X,  CANVAS_Y     = 0, 0     
local TW_X,      TW_Y         = 0, 0      
local WHISKY_X,  WHISKY_Y     = 0, 0         
local VELOCITY_X, VELOCITY_Y  = 0, 0   

local VELOCITY_NUDGE_X, VELOCITY_NUDGE_Y = 0, 0

local MAIN_X,    MAIN_Y       = 0, 0
local TACH_PULL, TACH_Y       = 0, 0
local RADAR_CIRCLE_DY         = 0 -- Mortem: keep circle/compass aligned with the minimap renderer.           

local VICON_BASE_X,      VICON_BASE_Y      = 1920, 1080
local GUNRETICLE_BASE_X, GUNRETICLE_BASE_Y = 1920, 1080

local SHIFT_VIA_PERSISTENT = true
local FIX_RADAR_CIRCLE     = true
local SINGLE_INSTANCE      = true

local RESET_LIST = {
    { key = "GunReticle",           pat = "GunReticle",           x = 1920, y = 1080, relativeCenter = true },
    { key = "Seeker",               pat = "Seeker",               x = 0,    y = 0    },
    { key = "Locator",              pat = "Locator",              x = 0,    y = 0    },
    { key = "ExplosivePredication", pat = "ExplosivePredication", x = -TW_X, y = -TW_Y },
    { key = "TargetSelectionDraw",  pat = "TargetSelectionDraw",  x = 0,    y = 0    },
}

local DESIGN_DX, DESIGN_DY = 1294, -640

local ANCHOR_EXP_X, ANCHOR_EXP_Y = 494, 1746
local SCALE_TOL    = 400   
local K_MAX        = 2400  
local TOL_UNITS    = 3     
local CAL_GAIN0    = 0.5   
local CAL_MAX_ITER = 40
local CAL_FAIL_MAX = 120  
local CAL_STEP_EVERY  = 2    
local ASSERT_EVERY    = 20   
local VALIDATE_EVERY  = 180   
local MISSING_RETRY_EVERY = 1200  
local FINGERPRINT_EVERY   = 1200  
local HB_EVERY       = 600  
local SWEEP_RESIDUAL = true 
local SWEEP_THRESHOLD = 600 
local FORCE_K_X, FORCE_K_Y = 0, 0  

local TACH_K_TABLE = {
    { rx = 689,  ry = 630,  dx = 0,    dy = 0    },  
    { rx = 1026, ry = 938,  dx = -330, dy = -174 },  
    { rx = -330, ry = -174, dx = -330, dy = -174 },  
}
local TACH_K_TOL = 60          
local TACH_KX, TACH_KY = 0, 0  
local TACH_K_LOCKED = false    
local TACH_K_MATCH  = "AC8 layout"
local TACH_NUDGE_X, TACH_NUDGE_Y = 0, 0  

local K_FALLBACK = {
    { pat = "fa%-?18",        kx = 954,  ky = 570 },  
    { pat = "f%-?18",         kx = 954,  ky = 570 },
    { pat = "superhornet",    kx = 954,  ky = 570 },
    { pat = "hornet",         kx = 954,  ky = 570 },
    { pat = "fa%-?14",        kx = 0,    ky = 0   },  
    { pat = "f%-?14",         kx = 0,    ky = 0   },
    { pat = "tomcat",         kx = 0,    ky = 0   },
}

local function p(obj)
    if obj == nil then return "<nil>" end
    local ok, v = pcall(function() return AC8Runtime.objectCall(obj,'get_full_name') end)
    if not ok or v == nil then return "<no full_name>" end
    return v
end
local function short_name(obj)
    local nm = p(obj)
    if type(nm) == "string" then return nm:match("([^%.%/]+)$") or nm end
    return "<?>"
end
local function readField(o,k)return o[k]end
local function get_field(obj, name)
    if obj == nil then return nil end
    local ok, v = pcall(readField,obj,name)
    if not ok then return nil end
    return v
end
local function get_rt_xy(obj)
    if not AC8Runtime.isLiveObject(obj)then return nil,nil end
    if obj == nil then return nil, nil end
    local rt = get_field(obj, "RenderTransform")
    if rt == nil then return nil, nil end
    local t = get_field(rt, "Translation")
    if t == nil then return nil, nil end
    return get_field(t, "X"), get_field(t, "Y")
end

local function fast_rt(obj, tx, ty)
    if obj == nil then return false end
    pcall(function() obj:SetRenderTranslation({ X = tx, Y = ty }) end)
    return true
end

local function set_rt(obj, tx, ty, tag, quiet)
    if obj == nil then
        if not quiet then  end
        return false
    end
    pcall(function() obj:SetRenderTranslation({ X = tx, Y = ty }) end)
    if not quiet then
        local nx, ny = get_rt_xy(obj)
        local good = (type(nx) == "number" and math.abs(nx - tx) < 1 and math.abs(ny - ty) < 1)
        
    end
    return true
end
local function iter_children(w)
    if AC8Runtime.inventory then return AC8Runtime.inventory.children(w)or{}end
    local children = {}
    if w == nil then return children end
    local okc, cc = pcall(function() return w.GetChildrenCount and w:GetChildrenCount() or 0 end)
    if okc and type(cc) == "number" and cc > 0 then
        for i = 0, cc - 1 do
            local ok2, c = pcall(function() return w:GetChildAt(i) end)
            if ok2 and c then table.insert(children, c) end
        end
    end
    if #children == 0 then
        local wt = get_field(w, "WidgetTree")
        if wt then
            local r = get_field(wt, "RootWidget")
            if r then table.insert(children, r) end
        end
    end
    return children
end
local function find_desc(root, target, maxd, exact)
    if root == nil then return nil end
    local seen, found = {}, nil
    local function rec(w, d)
        if w == nil or found or d > maxd then return end
        local addr = tostring(w)
        if seen[addr] then return end
        seen[addr] = true
        local nm = short_name(w)
        local hit = exact and (nm == target) or (not exact and nm:find(target, 1, true) ~= nil)
        if hit then found = w; return end
        for _, c in ipairs(iter_children(w)) do rec(c, d + 1) end
    end
    rec(root, 0)
    return found
end
local function find_path(root, addr, maxd)
    if root == nil or addr == nil then return nil end
    local path = nil
    local function rec(w, d, acc)
        if w == nil or path or d > maxd then return end
        table.insert(acc, w)
        if tostring(w) == addr then path = acc; return end
        for _, c in ipairs(iter_children(w)) do rec(c, d + 1, acc) end
        if path == nil then table.remove(acc) end
    end
    rec(root, 0, {})
    return path
end
local function all_userwidgets()
    if uevr == nil or uevr.api == nil then return {} end
    local umg = uevr.api:find_uobject("Class /Script/UMG.UserWidget")
    if umg == nil then return {} end
    local ok, objs = pcall(function() return AC8Runtime.inventory.objects(umg,false) end)
    if not ok or type(objs) ~= "table" then return {} end
    return objs
end
local function all_mainflights(uw)
    local out = {}
    for _, w in ipairs(uw) do
        local nm = p(w)
        if type(nm) == "string" and nm:find("WBP_HUD_Chronicle_MainFlight", 1, true)
            and not nm:find("Default__", 1, true) and not nm:find(".WidgetTree", 1, true) then
            table.insert(out, w)
        end
    end
    return out
end

local function abs_center(w)
    -- Calibration constants are authored HUD-local units. A physical absolute
    -- origin plus an unscaled local Size/2 is not a valid center under DPI.
    local slate,rootGeo,rootSize=rootGeometry()
    local g=simpleGeometry(w);local size=simpleXY(simpleCall(slate,'GetLocalSize',g))
    if not rootSize or not size or size.X<=0 or size.Y<=0 then return nil end
    local center=rootPoint(slate,rootGeo,g,{X=size.X/2,Y=size.Y/2})
    if center then return{x=center.X,y=center.Y,via='current HUD-root geometry'}end
    return nil
end

local function pawn_full_name()
    if uevr == nil or uevr.api == nil or uevr.api.get_player_pawn == nil then return nil end
    local ok, pawn = pcall(function() return uevr.api:get_player_pawn() end)
    if not ok or pawn == nil then return nil end
    local ok2, nm = pcall(function() return AC8Runtime.objectCall(pawn,'get_full_name') end)
    if ok2 and type(nm) == "string" then return nm end
    return nil
end
local function fallback_k(name)
    if name == nil then return nil end
    local low = string.lower(name)
    for _, it in ipairs(K_FALLBACK) do
        if low:find(it.pat) then return it.kx, it.ky end
    end
    return nil
end

local C = {
    ready  = false,
    keep   = nil, base = nil, wtsc = nil, maincv = nil, cp = nil,
    ts = nil, ta = nil, vicon = nil, vel = nil, whisky = nil,
    radar = {}, radar_main = nil,
    pins = {},          
}
local cal = {
    active = false, kx = 0, ky = 0, gain = CAL_GAIN0, iter = 0,
    scale = nil, pex = nil, pey = nil, cur_fp = nil, warm = 0, fail = 0,
}

local R0_X = CANVAS_X - TW_X
local R0_Y = CANVAS_Y - TW_Y

local function capture_profiles(zero_big, quiet)
    local out = {}
    if C.cp == nil then return out end
    for _, tgt in ipairs({ "SpeedTachometerCenterPanel", "AltitudeTachometerCenterPanel", "WhiskyMarkPanel" }) do
        local t = find_desc(C.cp, tgt, 14, true)
        if t ~= nil then
            local path = find_path(C.cp, tostring(t), 14)
            if path ~= nil and #path > 2 then
                local prof = { key = tgt, layers = {}, sx = 0, sy = 0, cleared = 0 }
                for i = 2, #path - 1 do
                    local w = path[i]
                    local x, y = get_rt_xy(w)
                    local x0 = (type(x) == "number") and x or 0
                    local y0 = (type(y) == "number") and y or 0

                    local mag = math.abs(x0) + math.abs(y0)
                    if zero_big and (C.resid == nil or mag > C.resid.mag) then
                        C.resid = { x = x0, y = y0, mag = mag }
                    end
                    table.insert(prof.layers, { name = short_name(w), x = x0, y = y0 })
                    if zero_big and (math.abs(x0) > SWEEP_THRESHOLD or math.abs(y0) > SWEEP_THRESHOLD) then
                        if not quiet then
                            
                        end
                        fast_rt(w, 0, 0)
                        prof.max_layer = { name = short_name(w), x = x0, y = y0 }
                        prof.cleared = prof.cleared + 1
                    else
                        prof.sx = prof.sx + x0
                        prof.sy = prof.sy + y0
                    end
                end
                table.insert(out, prof)
            end
        end
    end
    return out
end

local function sweep_residual(quiet)
    if not SWEEP_RESIDUAL or C.cp == nil then return 0 end
    C.resid = nil
    C.prof = capture_profiles(true, quiet)
    local fixed = 0
    if C.prof ~= nil then
        for _, prof in ipairs(C.prof) do fixed = fixed + (prof.cleared or 0) end
    end
    if not quiet then
        if fixed > 0 then
            
        else
            
        end
        if C.prof ~= nil then
            for _, prof in ipairs(C.prof) do
                
                if prof.max_layer ~= nil then
                    
                end
            end
            
        end
    end
    return fixed
end

local function match_tach_k()
    TACH_KX,TACH_KY=0,0;TACH_K_MATCH="Clean HUD baseline";do return end
    if TACH_K_LOCKED then
        TACH_K_MATCH = string.format("AC8 layout value=%d, value=%d", TACH_KX, TACH_KY)
        return
    end
    local r = C.resid
    if r == nil then
        TACH_KX, TACH_KY = 0, 0
        TACH_K_MATCH = "AC8 layout"
        return
    end
    for _, e in ipairs(TACH_K_TABLE) do
        if math.abs(r.x - e.rx) <= TACH_K_TOL and math.abs(r.y - e.ry) <= TACH_K_TOL then
            TACH_KX, TACH_KY = e.dx, e.dy
            TACH_K_MATCH = string.format("AC8 layout value=%.0f, value=%.0f, value=%d, value=%d", r.x, r.y, TACH_KX, TACH_KY)
            if TACH_KX ~= 0 or TACH_KY ~= 0 then TACH_K_LOCKED = true end
            return
        end
    end
    TACH_KX, TACH_KY = 0, 0
    TACH_K_MATCH = string.format("AC8 layout value=%.0f, value=%.0f", r.x, r.y)
end

local function collect_radar(root)
    local out = {}
    if root == nil then return out end
    local seen = {}
    local function rec(w, d)
        if w == nil or d > 18 then return end
        local addr = tostring(w)
        if seen[addr] then return end
        seen[addr] = true
        if short_name(w):find("RadarCircleCanvasPanel", 1, true) then
            table.insert(out, w)
        end
        for _, c in ipairs(iter_children(w)) do rec(c, d + 1) end
    end
    rec(root, 0)
    return out
end

local function pin_position(pin)
    if pin.relativeCenter then
        local x,y=simpleHUD.resetCenter(pin.obj,pin.x,pin.y)
        -- An unarranged parent is not evidence for a fixed-centre fallback.
        -- Retry via the existing ordinary pin check; add no new timer or scan.
        pin.centerPending=x==nil or y==nil
        return x,y
    end
    return pin.x,pin.y
end
local function assert_pins(slow)
    local n = 0
    for _, pin in ipairs(C.pins) do
        if pin.obj ~= nil and (slow or not pin.slow or pin.centerPending) then
            local x, y = pin_position(pin)
            if pin.dyn then x = R0_X + cal.kx; y = R0_Y + cal.ky end
            local cx, cy = get_rt_xy(pin.obj)
            local okx = (type(cx) == "number" and x~=nil and math.abs(cx - x) < 1)
            local oky = (type(cy) == "number" and y~=nil and math.abs(cy - y) < 1)
            if x~=nil and y~=nil and not (okx and oky) then
                fast_rt(pin.obj, x, y)
            end
            n = n + 1
        end
    end
    if FIX_RADAR_CIRCLE then
        for _, w in ipairs(C.radar) do
            if w ~= nil then
                local cx, cy = get_rt_xy(w)
                if not (type(cx) == "number" and math.abs(cx) < 1
                        and type(cy) == "number" and math.abs(cy - RADAR_CIRCLE_DY) < 1) then
                    fast_rt(w, 0, RADAR_CIRCLE_DY)
                end
                n = n + 1
            end
        end
    end
    
    if slow then
        if C.keep ~= nil then
            pcall(function() C.keep.RenderOpacity = 1.0 end)
            pcall(function() C.keep.Visibility = 0 end)
        end
        if C.base ~= nil then
            pcall(function() C.base.RenderOpacity = 1.0 end)
            pcall(function() C.base.Visibility = 0 end)
        end
    end
    return n
end

local function cal_use_fallback(why, name)
    local kx, ky = fallback_k(name)
    if kx == nil then kx = 0; ky = 0 end
    cal.kx, cal.ky = kx, ky
    cal.active = false
    
    assert_pins(true)   
    return true
end

local function cal_step()
    if not cal.active then return true end
    -- Keep the existing authored local placement and the user's saved layout.
    -- Radar and instrument paint geometry can refer to different retainer
    -- frames; using their centers as a reference displaced the entire ladder.
    cal.kx,cal.ky=0,0
    cal.active=false;cal.lastSample=nil
    return true
end

local function retry_missing()
    if C.wtsc == nil then return end
    for _, it in ipairs(RESET_LIST) do
        local pin = nil
        for _, q in ipairs(C.pins) do if q.tag == "RESET:" .. it.key then pin = q end end
        if pin ~= nil and pin.obj == nil then
            local w = find_desc(C.wtsc, it.pat, 12)
            if w ~= nil then
                pin.obj = w
                local x,y=pin_position(pin)
                if x~=nil and y~=nil then fast_rt(w, x, y)end
                
            end
        end
    end
end

local effect_originals={}
local effect_clear=true
local effect_retain=false
local effect_bright_ladder=false
local effect_bright_weapons=false
local effect_bright_all=false
local effect_text_originals={}
local effect_text_rows={}
local function effect_material_name(material)
    if material==nil then return "None" end
    local ok,name=pcall(function() return AC8Runtime.objectCall(material,'get_full_name') end)
    return ok and name or "Material reference unavailable"
end
local function effect_poll()
    local ok,cmd=pcall(function() return preference(json,"AC8_VR-WorkshopEffectControls.json") end)
    if ok and type(cmd)=="table" and type(cmd.clearMaterial)=="boolean" then effect_clear=cmd.clearMaterial end
    if ok and type(cmd)=="table" then
        if type(cmd.retainRendering)=="boolean" then effect_retain=cmd.retainRendering end
        if type(cmd.brightLadder)=="boolean" then effect_bright_ladder=cmd.brightLadder end
        if type(cmd.brightWeapons)=="boolean" then effect_bright_weapons=cmd.brightWeapons end
        if type(cmd.brightAllPanels)=="boolean" then effect_bright_all=cmd.brightAllPanels end
    end
end
local function effect_text_apply()
    local root=C.keep
    if not root then effect_text_rows={};return end
    local prefix=AC8Runtime.objectCall(root,'get_full_name'):match("%s(.+)$")
    if not prefix then return end
    if not effect_bright_ladder and not effect_bright_weapons and not effect_bright_all and not next(effect_text_originals) then
        effect_text_rows={};return
    end
    local cls=uevr.api:find_uobject("Class /Script/UMG.Widget")
    if not cls then return end
    local rows={}
    for _,o in ipairs(AC8Runtime.inventory.objects(cls,false) or {}) do
        local ok,err=pcall(function()
            local full=AC8Runtime.objectCall(o,'get_full_name');local path=full:match("%s(.+)$")
            if not path or path:sub(1,#prefix+1)~=prefix.."." then return end
            local leaf=path:match("([^.]+)$")
            local group=nil

            if path:find(".WBP_ChroniclePersistent.",1,true) or leaf=="WBP_ChroniclePersistent" then group="ladder"
            elseif path:find("PlayerState",1,true) then group="weapons"
            elseif path:lower():find("minimap",1,true) then group="radar"
            elseif path:find("CampaignInfo",1,true) then group="score-timer"
            elseif path:find(".HUDMessageWidget.",1,true) or leaf=="HUDMessageWidget" then group="comms-messages" end
            if not group then return end
            local enabled=effect_bright_all or (group=="ladder" and effect_bright_ladder) or (group=="weapons" and effect_bright_weapons)
            local base=effect_text_originals[full]
            if not enabled and not base then return end
            if not base then
                local opacity=o:GetRenderOpacity()
                assert(type(opacity)=="number","Original widget opacity unavailable")
                base={opacity=opacity};effect_text_originals[full]=base
            end
            local wanted=enabled and 1.0 or base.opacity
            if math.abs(o:GetRenderOpacity()-wanted)>0.001 then o:SetRenderOpacity(wanted) end
            local actual=o:GetRenderOpacity()
            rows[#rows+1]={name=leaf,path=path,group=group,original=base.opacity,current=actual,
                enabled=enabled,verified=math.abs(actual-wanted)<0.001}
            if not enabled then effect_text_originals[full]=nil end
        end)
        if not ok then rows[#rows+1]={error=tostring(err),verified=false} end
    end
    effect_text_rows=rows
end
local function effect_apply(blur)
    if not blur then return end
    local name=AC8Runtime.objectCall(blur,'get_full_name')
    local base=effect_originals[name]
    if not base then
        local ok,material=pcall(function() return blur:get_property("EffectMaterial") end)
        assert(ok,"Original effect material could not be read")
        local retain=blur:get_property("bRetainRender")
        assert(type(retain)=="boolean","Original retainer flag unavailable")
        base={material=material,retain=retain};effect_originals[name]=base
    end
    local wanted=nil
    if not effect_clear then wanted=base.material end
    local current=blur:get_property("EffectMaterial")
    if current~=wanted then blur:SetEffectMaterial(wanted) end
    local retainWanted=base.retain
    if effect_retain then retainWanted=true end
    if blur:get_property("bRetainRender")~=retainWanted then
        blur:SetRetainRendering(retainWanted)
        pcall(function() blur:RequestRender() end)
    end
    local actual=blur:get_property("EffectMaterial")
    local desiredName=effect_material_name(wanted)
    local actualName=effect_material_name(actual)
    local actualRetain=blur:get_property("bRetainRender")
    
end
effect_poll()

local refresh_remaining=0
local refresh_reason='startup'
local function queue_hud_refresh(reason)
    refresh_remaining=2
    refresh_reason=reason
end
local function refresh_hud_layout()
    if not C.ready or refresh_remaining<=0 then return end
    assert_pins(true)
    local invalidated=pcall(function() C.keep:InvalidateLayoutAndVolatility() end)
    local prepass=pcall(function() C.keep:ForceLayoutPrepass() end)
    local rendered=false
    if C.workshopBlur then rendered=pcall(function() C.workshopBlur:RequestRender() end) end
    refresh_remaining=refresh_remaining-1
    
end
uevr.sdk.callbacks.on_lua_event(function(name,data)
    if name=='AC8_HUD_Refresh' then queue_hud_refresh(data or 'glow_activation') end
end)

local function discover()
    
    
    C.keep = nil; C.base = nil; C.wtsc = nil; C.maincv = nil; C.cp = nil
    C.ts = nil; C.ta = nil; C.vicon = nil; C.vel = nil; C.whisky = nil
    C.radar = {}; C.radar_main = nil; C.pins = {}
    C.resid = nil; C.prof = nil

    local uw = all_userwidgets()
    
    local mfs = all_mainflights(uw)
    
    if #mfs == 0 then
        
        return false
    end
    local keep, keep_addr = nil, nil
    for _, mf in ipairs(mfs) do
        local inv = nil
        pcall(function() inv = mf:IsInViewport() end)
        if inv == true then keep = mf; keep_addr = tostring(mf); break end
    end
    if AC8Runtime.context.rootName:match('^WBP_HUD_Chronicle_MainFlight_000_C ') then keep=AC8Runtime.context.root;keep_addr=tostring(keep);pcall(function()if not keep:IsInViewport()then AC8Runtime.inventory.call(keep,'AddToViewport')end end)end
    if keep == nil then
        keep = mfs[1]; keep_addr = tostring(mfs[1])
        pcall(function() AC8Runtime.inventory.call(keep,'AddToViewport') end)
    end
    if #mfs > 1 and SINGLE_INSTANCE then
        for _, mf in ipairs(mfs) do
            if tostring(mf) ~= keep_addr then
                local inv = nil
                pcall(function() inv = mf:IsInViewport() end)
                if inv == true then pcall(function() mf:RemoveFromViewport() end) end
            end
        end
    end
    C.keep = keep

    pcall(function() keep.RenderOpacity = 1.0 end)
    pcall(function() keep.Visibility = 0 end)

    local base = find_desc(keep, "BaseCanvas", 6, true)
    if base == nil then
        
        return false
    end
    C.base = base
    pcall(function() base.RenderOpacity = 1.0 end)
    pcall(function() base.Visibility = 0 end)

    local blur = find_desc(base, "BlurEffectBox", 8)
    if blur then
        C.workshopBlur=blur
        pcall(effect_apply,blur)
        pcall(function() blur.Visibility = 0 end)
        pcall(function() blur.RenderOpacity = 1.0 end)
    end

    local wtsc = find_desc(base, "WorldToScreenDrawCanvas", 3, true)
    if wtsc == nil then
        
        return false
    end
    C.wtsc = wtsc
    C.maincv = find_desc(base, "MainCanvas", 3, true)
    C.cp = find_desc(wtsc, "ChroniclePersistent", 3)
    C.vicon  = find_desc(wtsc, "VelocityIcon", 12, true)
    C.vel    = find_desc(wtsc, "Velocity", 12, true)
    C.whisky = find_desc(wtsc, "WhiskyMarkPanel", 12, true)
    C.ts     = find_desc(wtsc, "SpeedTachometerCenterPanel", 14, true)
    C.ta     = find_desc(wtsc, "AltitudeTachometerCenterPanel", 14, true)
    C.radar  = collect_radar(C.maincv)
    if #C.radar > 0 then
        
        for _, w in ipairs(C.radar) do
            local v = get_field(w, "Visibility")
            if v == nil or v == 0 or v == 3 or v == 4 then C.radar_main = w; break end
        end
        if C.radar_main == nil then C.radar_main = C.radar[1] end
    end
    AC8Runtime.hudLayoutRefs={radar=C.radar_main,root=C.keep}

    sweep_residual(false)
    match_tach_k()
    

    table.insert(C.pins, { obj = C.wtsc,   x = TW_X, y = TW_Y, tag = "AC8 layout" })
    if C.cp ~= nil and SHIFT_VIA_PERSISTENT then
        table.insert(C.pins, { obj = C.cp, x = R0_X, y = R0_Y, tag = "AC8 layout", dyn = true })
    end
    if C.maincv ~= nil then
        table.insert(C.pins, { obj = C.maincv, x = MAIN_X, y = MAIN_Y, tag = "AC8 layout" })
    end

    if C.whisky ~= nil then table.insert(C.pins, { obj = C.whisky, x = WHISKY_X, y = WHISKY_Y, tag = "WhiskyMarkPanel(W)" }) end
    if C.ts     ~= nil then table.insert(C.pins, { obj = C.ts,     x = TACH_PULL + TACH_KX + TACH_NUDGE_X, y = TACH_Y + TACH_KY + TACH_NUDGE_Y, tag = "AC8 layout" }) end
    if C.ta     ~= nil then table.insert(C.pins, { obj = C.ta,     x = -TACH_PULL + TACH_KX + TACH_NUDGE_X, y = TACH_Y + TACH_KY + TACH_NUDGE_Y, tag = "AC8 layout" }) end
    for _, it in ipairs(RESET_LIST) do
        local w = find_desc(wtsc, it.pat, 12)
        table.insert(C.pins, { obj = w, x = it.x, y = it.y, relativeCenter=it.relativeCenter, tag = "RESET:" .. it.key, slow = true })
    end

    for _, pin in ipairs(C.pins) do
        if pin.obj ~= nil and not pin.dyn then
            local x,y=pin_position(pin)
            if x~=nil and y~=nil then set_rt(pin.obj, x, y, pin.tag, false)end
        end
    end
    if C.cp ~= nil and SHIFT_VIA_PERSISTENT then
        set_rt(C.cp, R0_X + cal.kx, R0_Y + cal.ky, "AC8 layout", false)
    else
        
        table.insert(C.pins, { obj = C.wtsc, x = CANVAS_X, y = CANVAS_Y, tag = "AC8 layout" })
        
    end
    if FIX_RADAR_CIRCLE then
        for _, w in ipairs(C.radar) do
            set_rt(w, 0, RADAR_CIRCLE_DY, "RADARCIRCLE:" .. short_name(w), false)
        end
        
    end
    pcall(function() wtsc.Visibility = 0 end)
    pcall(function() wtsc.RenderOpacity = 1.0 end)
    if C.maincv ~= nil then
        pcall(function() C.maincv.Visibility = 0 end)
        pcall(function() C.maincv.RenderOpacity = 1.0 end)
    end

    
    for _, e in ipairs({ { C.radar_main, "AC8 layout" }, { C.ts, "AC8 layout" },
                         { C.ta, "AC8 layout" }, { C.vel, "AC8 layout" }, { C.whisky, "W" } }) do
        local c = abs_center(e[1])
        if c ~= nil then
            
        else
            
        end
    end

    cal.kx, cal.ky = 0, 0
    cal.gain = CAL_GAIN0; cal.iter = 0; cal.scale = nil; cal.pex = nil; cal.pey = nil
    cal.lastSample=nil;cal.warm = 3
    cal.active = true
    assert_pins(true)   

    C.ready = true
    queue_hud_refresh("initial_layout")
    
    return true
end

local function validate()
    for _, e in ipairs({ C.keep, C.base, C.wtsc, C.cp }) do
        if e ~= nil then
            local ok, v = pcall(function() return AC8Runtime.objectCall(e,'get_full_name') end)
            if not ok or v == nil or v == "<no full_name>" then
                
                C.ready = false
                return false
            end
        end
    end
    return true
end

local function fingerprint_check()
    local nm = pawn_full_name()
    if nm == nil then return end
    if cal.cur_fp == nil then
        cal.cur_fp = nm
        
        return
    end
    if nm ~= cal.cur_fp then
        
        cal.cur_fp = nm
        cal.kx, cal.ky = 0, 0
        cal.gain = CAL_GAIN0; cal.iter = 0; cal.scale = nil; cal.pex = nil; cal.pey = nil
        cal.lastSample=nil;cal.warm = 3
        cal.active = true
        assert_pins()
    end
end



local frame_n = 0
local retry_gap = 1      
local function on_frame()
    R.gameplayCameraReady=false
    local intent=rawget(_G,'AC8_CinematicIntent')
    if intent then intent.hudAcknowledged=true end
    local context=AC8Runtime.context
    if C.hudEpoch~=context.epoch then C.hudEpoch=context.epoch;C.ready=false;retry_gap=1;frame_n=0 end
    if not gameplay(AC8Runtime)then return end
    frame_n = frame_n + 1
    if not C.ready then
        if frame_n % retry_gap == 0 then
            if discover() then
                retry_gap = 1
            else
                retry_gap = math.min(retry_gap * 2, 30)
            end
        end
        return
    end
    if cal.active and (frame_n % CAL_STEP_EVERY == 0) then
        cal_step()
        if not cal.active then queue_hud_refresh("layout_settled") end
    end
    if frame_n % 8 == 0 then refresh_hud_layout() end
    if frame_n % ASSERT_EVERY == 0 then assert_pins() end          
    if frame_n % VALIDATE_EVERY == 0 then if not validate() then return end end
    if frame_n % FINGERPRINT_EVERY == 0 then fingerprint_check() end
    if frame_n % MISSING_RETRY_EVERY == 0 then retry_missing() end
    
    -- Corrective startup sweep and existing lifetime/recreation checks remain.   
    if frame_n % HB_EVERY == 0 then
        assert_pins(true)   
        local ax, ay = get_rt_xy(C.cp)
        
    end
    R.gameplayCameraReadyEpoch=AC8Runtime.context.epoch
    R.gameplayCameraReady=true
end


local ok0, err0 = pcall(discover)
if not ok0 then  end

local function register_tick()
    local cbs = uevr and uevr.sdk and uevr.sdk.callbacks
    if cbs and cbs.on_post_engine_tick then
        local ok2, e2 = pcall(function() cbs.on_post_engine_tick(on_frame) end)
        if ok2 then ; return true end
        
    end
    return false
end
local okr, errr = pcall(register_tick)
if not okr then  end




































local effect_ticks=0
uevr.sdk.callbacks.on_post_engine_tick(function()
    if not gameplay(AC8Runtime)then return end
    effect_ticks=effect_ticks+1
    if effect_ticks%60~=0 then return end

    local textOk,textErr=pcall(effect_text_apply)
    if not textOk then effect_text_rows={{verified=false,error=tostring(textErr)}} end
    local ok,err=pcall(effect_apply,C.workshopBlur)
    if not ok then
        
    end
end)

end)(AC8Runtime.host('layout','AC8_VR-HUDInspectV129.lua'),AC8Runtime.json,AC8Runtime.log)
end
end

AC8Runtime.inheritedSetup[#AC8Runtime.inheritedSetup+1]=function()

do
-- Flat2VR Modding Community and Discord: inherited ac8_ladder_fix_v19_fast, including the original groups described as Herbert's in the supplied code. Preserve ladder, radar, aircraft and canvas controls and numeric originals; apply saved workshop revisions. HUD update ordering is owned by ac8_20_hud_layout.lua.
(function(uevr,json,print)


local api = uevr.api
if api == nil then return end

local SCRIPT_VERSION = "v19"

local TARGETS = {
    {
        key    = "ladder",
        label  = "PitchLadder",
        from   = { "PitchMeter" },         
        nodes  = {
            "Altimeter",      

        },

        sublabel = "Heading",
        x = 1920.0, y = 1080.0, active = true,  op = 1.0, sc = 1.0,
        
        sdx = -1920.0, sdy = -1080.0,
    },
    {

        key    = "radar",
        label  = "Radar",
        from   = { "MiniMap", "Radar" },
        nodes  = {

            "MiniMapLayer",
            "MIniMapCanvas",
            "RadarCircleCanvasPanel",     
            "RadarCircleBase",            
            "MiniMapIconCanvasPanel",
            "MiniMapCornerCanvas",
            "MiniMapSearchCorners",
            "MiniMapEnlargedCorners",
        },
        x = 0.0, y = 0.0, active = false, op = 1.0, sc = 1.0,
    },
    {
        
        key    = "info",
        label  = "AircraftInfo",
        from   = { "PlayerState", "PlayerStatus" },
        nodes  = {

            "WBP_PlayerStateWidget",      
            "PlaneIconStatPanel",         
            "PlaneCanvas",
            "PlaneTextStatPanel",         
            "PlayerNamePanel",
            "Weapons",                    
            "WeaponsADMM",
            "Flares",
            "IconsCanvas",
        },
        x = 0.0, y = 0.0, active = false, op = 1.0, sc = 1.0,
    },
    {

        key    = "drawcanvas",
        label  = "Draw canvas",
        from   = { "WorldToScreen", "DynamicDraw" },
        nodes  = {
            "LowestPriorityDynamicDrawCanvas",
            "LowestPriorityCanvasWorldToScreenDraw",
            "WorldToScreenDrawCanvas",
            "TopPriorityCanvas",
            "MainCanvas",
            "DynamicDrawCanvas",
        },
        nopos = true,
        x = 0.0, y = 0.0, active = false, op = 1.0, sc = 1.0,
    },
}

local SCOPE = "WBP_HUD_Chronicle_MainFlight_000_C"

local DEFAULTS = {}
for _, t in ipairs(TARGETS) do
    DEFAULTS[t.key] = { x = t.x, y = t.y, active = t.active, op = t.op, sc = t.sc,
                        sdx = t.sdx or 0.0, sdy = t.sdy or 0.0 }
end

local RESCAN_EVERY = 1800

local CFG_FILE = "AC8_VR-Ladder.json"
local dirty, save_msg = false, ""

local function readField(o,k)return o[k]end
local function call(obj, fn, ...)
    local I=AC8Runtime.inventory
    if I and I.handles(fn,select(1,...))then
        local value=I.call(obj,fn,...);if value~=nil then return value end
    end
    if I and I.mutates(fn)then I.invalidate(fn)end
    local ok, f = pcall(readField,obj,fn)
    if ok and f ~= nil then
        local ok2, r = pcall(f, obj, ...)
        if ok2 then return r end
    end
    local ok3, f2 = pcall(function() return obj.call end)
    if ok3 and f2 ~= nil then
        local ok4, r2 = pcall(f2, obj, fn, ...)
        if ok4 then return r2 end
    end
    return nil
end

local function full_name(o)
    local ok, s = pcall(function() return AC8Runtime.objectCall(o,'get_full_name') end)
    if ok and type(s) == "string" then return s end
    return nil
end

local function leaf(o)
    local s = full_name(o)
    if s ~= nil then return s:match("([^.]+)$") or s end
    return "?"
end

local function path_of(o)
    local s = full_name(o)
    if s == nil then return nil end
    return s:match("^%S+%s+(.*)$") or s
end

local RT_XN, RT_YN = { "X", "x" }, { "Y", "y" }
local function rt_index(o, k) return o[k] end
local function rt_read(o) return o:get_property("RenderTransform") end
local function num_field(o, names)
    for _, n in ipairs(names) do
        local ok, v = pcall(rt_index, o, n)
        if ok and type(v) == "number" then return v end
    end
    return nil
end

local function get_rt_xy(obj)
    if not AC8Runtime.isLiveObject(obj)then return nil,nil end
    local ok1, rt = pcall(rt_read, obj)
    if not ok1 or rt == nil then return nil, nil end
    local ok2, tr = pcall(rt_index, rt, "Translation")
    if not ok2 or tr == nil then return nil, nil end
    return num_field(tr, RT_XN), num_field(tr, RT_YN)
end

local function get_rt_scale(obj)
    if not AC8Runtime.isLiveObject(obj)then return nil,nil end
    local ok1, rt = pcall(rt_read, obj)
    if not ok1 or rt == nil then return nil, nil end
    local ok2, sc = pcall(rt_index, rt, "Scale")
    if not ok2 or sc == nil then return nil, nil end
    return num_field(sc, RT_XN), num_field(sc, RT_YN)
end
local function get_rt_all(obj)
    if not AC8Runtime.isLiveObject(obj)then return nil,nil,nil,nil end
    local ok1, rt = pcall(rt_read, obj)
    if not ok1 or rt == nil then return nil,nil,nil,nil end
    local x, y, sx, sy
    local ok2, tr = pcall(rt_index, rt, "Translation")
    if ok2 and tr ~= nil then x, y = num_field(tr, RT_XN), num_field(tr, RT_YN) end
    local ok3, sc = pcall(rt_index, rt, "Scale")
    if ok3 and sc ~= nil then sx, sy = num_field(sc, RT_XN), num_field(sc, RT_YN) end
    return x, y, sx, sy
end

local SEARCH_CLASSES = {}
local UW = nil
do
    for _, path in ipairs({ "Class /Script/UMG.UserWidget",
                            "Class /Script/UMG.CanvasPanel" }) do
        local ok, c = pcall(function() return api:find_uobject(path) end)
        if ok and c ~= nil then
            SEARCH_CLASSES[#SEARCH_CLASSES + 1] = c
            if path:find("UserWidget", 1, true) then UW = c end
        end
    end
end
if #SEARCH_CLASSES == 0 then
    
end

local function find_all_by_name(node_names)
    local out = {}
    local want = {}
    for _, n in ipairs(node_names) do want[n] = true end

    local seen = {}
    for _, cls in ipairs(SEARCH_CLASSES) do
        local ok, arr = pcall(function() return AC8Runtime.inventory.objects(cls,false) end)
        if ok and type(arr) == "table" then
            for _, w in ipairs(arr) do
                local okn, nm = pcall(function() return AC8Runtime.objectCall(w,'get_full_name') end)
                if okn and type(nm) == "string"
                    and nm:find("/Engine/Transient", 1, true)
                    and nm:find(SCOPE, 1, true)      
                then
                    local l = nm:match("([^.]+)$") or ""

                    local base = l
                    repeat
                        local prev = base
                        base = base:gsub("_%d+$", "")
                        if base == prev then break end
                    until false
                    if (want[l] or want[base]) and not seen[nm] then
                        seen[nm] = true
                        out[#out + 1] = w
                    end
                end
            end
        end
    end
    return out
end

local function is_under(child_path, parent_path)
    if child_path == nil or parent_path == nil then return false end
    if #parent_path >= #child_path then return false end
    return child_path:sub(1, #parent_path) == parent_path
end

local function prune_nested(list)
    if #list < 2 then return list end
    local paths = {}
    for i, o in ipairs(list) do paths[i] = path_of(o) end

    local keep = {}
    for i, o in ipairs(list) do
        local p, nested = paths[i], false
        if p ~= nil then
            for j = 1, #paths do
                if j ~= i and is_under(p, paths[j]) then nested = true; break end
            end
        end
        if not nested then keep[#keep + 1] = o end
    end
    return keep
end

local function find_by_walk(from_patterns, node_names)
    if UW == nil then return nil, {} end
    local ok, arr = pcall(function() return AC8Runtime.inventory.objects(UW,false) end)
    if not ok or type(arr) ~= "table" then return nil, {} end

    local want_node = {}
    for _, n in ipairs(node_names) do want_node[n] = true end

    for _, w in ipairs(arr) do
        local okn, nm = pcall(function() return AC8Runtime.objectCall(w,'get_full_name') end)
        if okn and type(nm) == "string" and nm:find("/Engine/Transient", 1, true) then
            local hit_from = false
            for _, p in ipairs(from_patterns) do
                if nm:find(p, 1, true) then hit_from = true; break end
            end
            if hit_from then
                local chain, cur = {}, w
                for _ = 1, 12 do
                    cur = call(cur, "GetParent")
                    if cur == nil then break end
                    local l = leaf(cur)
                    chain[#chain + 1] = l
                    if want_node[l] then return cur, chain end
                end
                return nil, chain
            end
        end
    end
    return nil, {}
end

local function prepare_items(t)
    t.items = nil
    if t.objs == nil then return end
    local out = {}
    for i, o in ipairs(t.objs) do
        local l = leaf(o)
        local base = l:gsub("_%d+$", "")
        local issub = false
        if t.subnames ~= nil then
            for _, nm in ipairs(t.subnames) do
                if l == nm or base == nm then issub = true; break end
            end
        end
        out[i] = { o = o, k = path_of(o) or l, fn = full_name(o), sub = issub }
    end
    t.items = out

    t.opApplied, t.scApplied = nil, nil
end

local function find_node(from_patterns, node_names)
    local list = find_all_by_name(node_names)
    if #list == 0 then
        
        local w = find_by_walk(from_patterns, node_names)
        list = (w ~= nil) and { w } or {}
    end
    return prune_nested(list)
end

local ORIG = {}

local function key_of(o)
    return path_of(o) or leaf(o)
end

local function remember(e)
    local r = ORIG[e.k]
    if r == nil then
        local ox, oy = get_rt_xy(e.o)
        local sx, sy = get_rt_scale(e.o)
        ORIG[e.k] = { x = ox, y = oy, sx = sx, sy = sy }
    end
end

local function restore(e)
    if not AC8Runtime.isLiveObject(e.o)then return end
    local r = ORIG[e.k]
    if r == nil then return end
    local o = e.o

    local cx, cy = get_rt_xy(o)
    if type(r.x) == "number" and type(r.y) == "number"
        and not (type(cx) == "number" and math.abs(cx - r.x) < 1
                 and type(cy) == "number" and math.abs(cy - r.y) < 1) then
        pcall(function() o:SetRenderTranslation({ X = r.x, Y = r.y }) end)
    end

    local sx, sy = get_rt_scale(o)
    if type(r.sx) == "number" and type(r.sy) == "number"
        and not (type(sx) == "number" and math.abs(sx - r.sx) < 0.01
                 and type(sy) == "number" and math.abs(sy - r.sy) < 0.01) then
        pcall(function() o:SetRenderScale({ X = r.sx, Y = r.sy }) end)
    end

    local ok, cur = pcall(function() return o:GetRenderOpacity() end)
    if ok and type(cur) == "number" and math.abs(cur - 1.0) > 0.01 then
        pcall(function() o:SetRenderOpacity(1.0) end)
    end
end

local function target_position(t,e)
    local x=t.x+(e.sub and(t.sdx or 0)or 0)
    local y=t.y+(e.sub and(t.sdy or 0)or 0)
    if t.key=='ladder'and not e.sub then return simpleHUD.ladderPosition(e.o,x,y)end
    return x,y
end
local function apply_one(t)
    if not gameplay(AC8Runtime)or t.items == nil then return end

    if not t.active then

        if t.applied then
            for _, e in ipairs(t.items) do restore(e) end
            t.applied = false
        end
        
        t.opApplied, t.scApplied = nil, nil
        return
    end

    local op_dirty = (type(t.op) == "number" and t.op ~= t.opApplied)
    local sc_dirty = (type(t.sc) == "number" and t.sc ~= t.scApplied)

    for _, e in ipairs(t.items) do
        local o = e.o
        if not AC8Runtime.isLiveObject(o)then t.objs=nil;t.items=nil;return end
        if t.key=='ladder'and not e.sub then simpleHUD.prepareLadder(o)end
        remember(e)               

        if not t.nopos then
            
            local tx, ty = target_position(t,e)

            local cx, cy = get_rt_xy(o)
            
            if tx and ty and not (type(cx) == "number" and math.abs(cx - tx) < 1
                    and type(cy) == "number" and math.abs(cy - ty) < 1) then
                pcall(function() o:SetRenderTranslation({ X = tx, Y = ty }) end)
            end
        end

        if op_dirty then
            pcall(function() o:SetRenderOpacity(t.op) end)
        end
        if sc_dirty then
            pcall(function() o:SetRenderScale({ X = t.sc, Y = t.sc }) end)
        end
    end

    if op_dirty then t.opApplied = t.op end
    if sc_dirty then t.scApplied = t.sc end
    t.applied = true     
end

local workshop_after, workshop_poll, workshop_ui, ws_simple_missing, drawHUDProfiles
local workshop_active=false
local function apply()
    for _, t in ipairs(TARGETS) do
        local ok, err = pcall(apply_one, t)
        if not ok then
            
        end
    end
    if workshop_after then pcall(workshop_after) end
end

local HAS_JSON = false
pcall(function() HAS_JSON = (json ~= nil and json.dump_file ~= nil) end)

local function cfg_by_key(k)
    for _, t in ipairs(TARGETS) do
        if t.key == k then return t end
    end
    return nil
end

local function save_cfg()
    if not HAS_JSON then return false, "no json api" end
    local out = {}
    for _, t in ipairs(TARGETS) do
        out[t.key] = { x = t.x, y = t.y, active = t.active, op = t.op, sc = t.sc,
                       sdx = t.sdx or 0.0, sdy = t.sdy or 0.0 }
    end
    local ok = pcall(function() json.dump_file(CFG_FILE, out, 4) end)
    return ok, ok and "saved" or "save FAILED"
end

local function apply_cfg_entry(tg, v)
    if type(v) ~= "table" then return false end
    if type(v.x) == "number" then tg.x = v.x end
    if type(v.y) == "number" then tg.y = v.y end
    if type(v.active) == "boolean" then tg.active = v.active end
    if type(v.op) == "number" then tg.op = v.op end
    if type(v.sc) == "number" then tg.sc = v.sc end
    if type(v.sdx) == "number" then tg.sdx = v.sdx end
    if type(v.sdy) == "number" then tg.sdy = v.sdy end
    tg.opApplied, tg.scApplied = nil, nil
    return true
end

local function read_cfg()
    do return nil end -- TEMP baseline: legacy layout file stays on disk.
    if not HAS_JSON then return nil end
    local ok, t = pcall(function() return json.load_file(CFG_FILE) end)
    if not ok or type(t) ~= "table" then return nil end
    return t
end

local function load_cfg()
    local t = read_cfg()
    if t == nil then return false end
    local n = 0
    for k, v in pairs(t) do
        local tg = cfg_by_key(k)
        if tg ~= nil and apply_cfg_entry(tg, v) then n = n + 1 end
    end
    return n > 0
end

local function load_cfg_one(tg)
    local all = read_cfg()
    if all == nil then return false end
    local v = all[tg.key]
    if v == nil then return false end
    return apply_cfg_entry(tg, v)
end

local function resolve_all(verbose)
    for _, t in ipairs(TARGETS) do
        if t.objs == nil or verbose then
            t.objs = find_node(t.from, t.nodes)
            prepare_items(t)
            if verbose then
                if #t.objs > 0 then
                    
                    for i, o in ipairs(t.objs) do
                        
                    end
                else
                    local _, chain = find_by_walk(t.from, t.nodes)
                    
                end
            end
        end
    end
end

local function rescan_all()
    local changed = {}
    for _, t in ipairs(TARGETS) do
        local before = t.items and t.items[1] and t.items[1].k or nil
        t.objs = find_node(t.from, t.nodes)
        prepare_items(t)
        local after = t.items and t.items[1] and t.items[1].k or nil
        if after ~= before then
            changed[#changed + 1] = string.format("%s(%d)", t.label,
                t.objs ~= nil and #t.objs or 0)
        end
    end
    if #changed > 0 then
        
    end
end

local rescan_idx = 1
local function rescan_one()
    local n = #TARGETS
    if n == 0 then return end
    local t = TARGETS[rescan_idx]
    rescan_idx = rescan_idx % n + 1
    local before = t.items and t.items[1] and t.items[1].k or nil
    t.objs = find_node(t.from, t.nodes)
    prepare_items(t)
    local after = t.items and t.items[1] and t.items[1].k or nil
    if after ~= before then
        
    end
end

local HAS_BUTTON = false
pcall(function() HAS_BUTTON = (imgui ~= nil and imgui.button ~= nil) end)

local HAS_SAME_LINE = false
pcall(function() HAS_SAME_LINE = (imgui ~= nil and imgui.same_line ~= nil) end)

local function nudge_row(t, field, label)
    local ok1, c1 = pcall(function() return imgui.button(label .. " -1") end)
    if ok1 and c1 then
        t[field] = (t[field] or 0.0) - 1.0
        t.active = true; dirty = true
    end
    if HAS_SAME_LINE then pcall(function() imgui.same_line() end) end
    local ok2, c2 = pcall(function() return imgui.button(label .. " +1") end)
    if ok2 and c2 then
        t[field] = (t[field] or 0.0) + 1.0
        t.active = true; dirty = true
    end
end

local function ui_body()
    if workshop_active and workshop_ui then workshop_ui(); return end
    if drawHUDProfiles then drawHUDProfiles()end
    imgui.text("AC8 HUD tune   " .. SCRIPT_VERSION)

    for _, t in ipairs(TARGETS) do

        local vtag = ""
        if t.objs ~= nil and t.objs[1] ~= nil then
            local okv, vv = pcall(function() return t.objs[1]:GetVisibility() end)
            if okv and type(vv) == "number" then
                if vv == 1 then vtag = "  [HIDDEN]"
                elseif vv == 2 then vtag = "  [HIDDEN vis=2]"
                else vtag = "  [vis=" .. vv .. "]" end
            end
        end
        imgui.text("  " .. t.label .. vtag
            .. (t.active and "   [modified]" or "")
            .. ((t.objs and #t.objs > 0) and "" or "   (not found)"))

        if t.objs ~= nil and #t.objs > 0 then
            
            local c3, v3 = imgui.slider_float(t.label .. " opacity", t.op or 1.0, 0.0, 1.0)
            if c3 then t.op = v3; t.active = true; dirty = true end

            local c4, v4 = imgui.slider_float(t.label .. " scale", t.sc or 1.0, 0.1, 4.0)
            if c4 then t.sc = v4; t.active = true; dirty = true end

            if not t.nopos then
                local c1, v1 = imgui.slider_float(t.label .. " X", t.x, -4000.0, 4000.0)
                if c1 then t.x = v1; t.active = true; dirty = true end
                local c2, v2 = imgui.slider_float(t.label .. " Y", t.y, -4000.0, 4000.0)
                if c2 then t.y = v2; t.active = true; dirty = true end
                nudge_row(t, "x", t.label .. " X")
                nudge_row(t, "y", t.label .. " Y")
            end

            if t.subnames ~= nil then
                local subX = t.label .. " " .. (t.sublabel or "sub") .. " X"
                local subY = t.label .. " " .. (t.sublabel or "sub") .. " Y"
                local cs1, vs1 = imgui.slider_float(subX, t.sdx or 0.0, -4000.0, 4000.0)
                if cs1 then t.sdx = vs1; t.active = true; dirty = true end
                nudge_row(t, "sdx", subX)

                local cs2, vs2 = imgui.slider_float(subY, t.sdy or 0.0, -4000.0, 4000.0)
                if cs2 then t.sdy = vs2; t.active = true; dirty = true end
                nudge_row(t, "sdy", subY)
            end

            local okr, clicked = pcall(function()
                return imgui.button("Load saved  " .. t.label)
            end)
            if okr and clicked then
                if load_cfg_one(t) then
                    dirty = false
                    
                else
                    
                end
            end
        end
    end

    local okb, saved = pcall(function() return imgui.button("Save") end)
    if okb and saved then
        local ok, msg = save_cfg()
        save_msg = (ok and "[OK] " or "[FAIL] ") .. msg
        if ok then dirty = false end
        
    end

    local okrs, reset = pcall(function() return imgui.button("Reset ALL to base") end)
    if okrs and reset then
        for _, t in ipairs(TARGETS) do
            local d = DEFAULTS[t.key]
            if d ~= nil then
                t.x, t.y, t.active, t.op, t.sc = d.x, d.y, d.active, d.op, d.sc
                t.sdx, t.sdy = d.sdx, d.sdy
                t.opApplied, t.scApplied = nil, nil
            end
        end
        local ok = save_cfg()
        save_msg = ok and "[RESET] all back to base" or "[RESET] save FAILED"
        if ok then dirty = false end
        
    end

    local okrl, reload = pcall(function()
        return imgui.button("Reload targets (after menu / mission change)")
    end)
    if okrl and reload then
        for _, t in ipairs(TARGETS) do
            t.objs, t.items = nil, nil
            t.applied = false
        end
        resolve_all(true)
        
    end

    imgui.text(dirty and "UNSAVED CHANGES" or "saved")
    if save_msg ~= "" then imgui.text(save_msg) end
end

local ui_err_logged = false
local function ui_safe()
    local ok, err = pcall(ui_body)
    if not ok and not ui_err_logged then
        ui_err_logged = true
        
    end
end

local ui_ok = pcall(function() uevr.sdk.callbacks.on_draw_ui(ui_safe) end)






resolve_all(true)

local layout_scope_sync
local function lt14_apply()apply()end
pcall(function()uevr.sdk.callbacks.on_pre_viewport_client_draw(function()
 if not layout_scope_sync or not layout_scope_sync()then return end
 for _,t in ipairs(TARGETS)do if t.key=='ladder'then pcall(apply_one,t)end end
end)end)

-- V66: widget reflection/writes run on the game thread. The old pre-Slate
-- callback could retain dead flight widgets and block the shared Lua lock.

local WS = {revision="none", groups={}, components={}, catalog={}, entries={}, originals={},
    groupbase={}, states={}, elapsed=0, scan_elapsed=15, rescan=-1, restore=false}
local WS_GROUPS = {ladder=true, radar=true, info=true}
for _, t in ipairs(TARGETS) do
    if WS_GROUPS[t.key] then
        WS.groupbase[t.key] = {active=t.active,x=t.x,y=t.y,sc=t.sc,op=t.op,
            sdx=t.sdx,sdy=t.sdy}
    end
end
local function ws_params(p)
    return type(p)=="table" and type(p.enabled)=="boolean"
        and type(p.x)=="number" and p.x>=-4000 and p.x<=4000
        and type(p.y)=="number" and p.y>=-4000 and p.y<=4000
        and type(p.scale)=="number" and p.scale>=0.1 and p.scale<=3
end
-- Script UI edits the same in-memory workshop settings as the game-thread
-- layout callback. No UObject reads/writes or extra scheduling in this UI.
local function ws_same_preferences(a,b)
    if type(a)~=type(b)then return false end
    if type(a)~='table'then return a==b end
    for k,v in pairs(a)do if not ws_same_preferences(v,b[k])then return false end end
    for k in pairs(b)do if a[k]==nil then return false end end
    return true
end
local ws_ui_dirty=false
local ws_ui_message=''
hudProfiles=AC8Runtime.aircraftPresets or AC8Runtime.hudDefaults:view()
local function profileSnapshot()
    return{coordinateRevision=2,weaponCalibration=simpleHUD.weapon,groups=WS.groups,components=WS.components,canvas=WS.canvas,simpleGroups=WS.simple,simpleTargetBase=simpleHUD.targetBase}
end
local function applyHUDProfile(layout,source)
    local cmd=hudProfiles.copy(layout);hudProfiles.serial=(hudProfiles.serial or 0)+1
    cmd.revision='hud-profile-'..hudProfiles.serial;cmd.rescan=hudProfiles.serial
    simpleHUD.layoutCommand=cmd
    WS.revision='profile-selection';WS.elapsed=19;WS.needScan=true;WS.simpleNextFind=0
    hudProfiles.lastSource=source;ws_ui_dirty=false;ws_ui_message=''
end
local function profilePlane()
    if not AC8Runtime.context.flight then return end
    local n=AC8Runtime.viewTargetName and AC8Runtime.viewTargetName()or''
    return n:match('^(BP_PlayerPlane_[%w_]+_C) ')
end
local function queueProfileAction(kind,value)
    hudProfiles.pending={kind=kind,layout=hudProfiles.copy(profileSnapshot()),value=value}
end
local function updateHUDProfiles()
    if not hudProfiles:init(hudProfiles.db==nil and simpleHUD.baselineLayout()or nil)then return end
    local plane=profilePlane();hudProfiles.currentPlane=plane
    if not AC8Runtime.aircraftPresets then
        local action=hudProfiles.pending;hudProfiles.pending=nil
        if action then
            if action.kind=='reset'then hudProfiles.force=true
            elseif action.kind=='hudGlobal'then local ok,msg=hudProfiles:saveGlobalHUD(action.layout);hudProfiles.message=msg;if ok then ws_ui_dirty=false end
            elseif action.kind=='all'then local ok,msg=hudProfiles:saveAll(action.layout);hudProfiles.message=msg;if ok then ws_ui_dirty=false end end
        end
    end
    local mode=hudProfiles.db.useDefaultForAllPlanes and'all'or'plane'
    local selection=tostring(hudProfiles.db.perPlaneEnabled)..':'..mode..':'..(plane or'none')
    if hudProfiles.force or(hudProfiles.selection~=selection and(plane or mode=='all'))then
        local layout,source=hudProfiles:selected(plane)
        if not layout or layout.coordinateRevision~=2 then layout=simpleHUD.baselineLayout();source='Neutral defaults (legacy HUD layout retained)'end
        if layout then applyHUDProfile(layout,source);hudProfiles.selection=selection;hudProfiles.force=false end
    end
end
local function uiSection(label)
    if imgui.spacing then imgui.spacing()end
    if imgui.separator then imgui.separator()else imgui.text('--------------------------------')end
    if label then imgui.text(label)end
    if imgui.spacing then imgui.spacing()end
end
drawHUDProfiles=function()
    if AC8Runtime.aircraftPresets then return end
    uiSection('Shared HUD layout')
    imgui.text('AC8_VR-AircraftPresets.lua is absent. General HUD controls remain available.')
    if imgui.button('Save current HUD as global default')then queueProfileAction('hudGlobal')end
    if imgui.button('Save HUD layout for ALL planes')then queueProfileAction('all')end
    if imgui.button('Reset HUD to saved layout')then queueProfileAction('reset')end
    if hudProfiles.message then imgui.text(hudProfiles.message)end
end
AC8Runtime.hudLayoutControls={snapshot=function()return hudProfiles.copy(profileSnapshot())end,saved=function()ws_ui_dirty=false;ws_ui_message=''end,changed=function()ws_ui_dirty=true end}

local function ws_ui_sync()
    for _,t in ipairs(TARGETS)do
        local p=WS.groups[t.key];local b=WS.groupbase[t.key]
        if p and b and b.active then
            t.x=b.x+(p.enabled and p.x or 0);t.y=b.y+(p.enabled and p.y or 0)
            t.sc=b.sc*(p.enabled and p.scale or 1);t.active=b.active;t.scApplied=nil
        end
    end
end
workshop_ui=function()
    local edited=false
    if drawHUDProfiles then drawHUDProfiles()end
    WS.simple=WS.simple or simpleHUD.normalize()
    WS.simple.fpm=WS.simple.fpm or{x=0,y=0}
    simpleHUD.values=WS.simple
    uiSection('Modify position of Grouped HUD Elements')
    imgui.text('Save/reload works for newly saved layouts.')
    imgui.text('Move complete groups. Negative X moves left; negative Y moves up.')
    local function controls(label,p,positionOnly,resetKeys)
        local function editedControl()
            if p.enabled~=nil then p.enabled=true end
            ws_ui_dirty=true;edited=true
        end
        if imgui.button(label..' reset offsets')then
            p.x,p.y=0,0
            for _,key in ipairs(resetKeys or{})do WS.simple[key].x,WS.simple[key].y=0,0 end
            editedControl()
        end
        for _,axis in ipairs({'x','y'})do
            local text=label..' '..axis:upper()
            local c,v=imgui.slider_float(text,p[axis],-4000,4000)
            if c then p[axis]=v;editedControl() end
            if imgui.button(text..' -1')then p[axis]=math.max(-4000,p[axis]-1);editedControl() end
            if imgui.same_line then imgui.same_line()end
            if imgui.button(text..' +1')then p[axis]=math.min(4000,p[axis]+1);editedControl() end
            if imgui.spacing then imgui.spacing()end
        end
        if positionOnly~=true then
            local c,v=imgui.slider_float(label..' scale',p.scale,.1,3)
            if c then p.scale=v;editedControl() end
            if imgui.button(label..' scale -0.01')then p.scale=math.max(.1,p.scale-.01);editedControl() end
            if imgui.same_line then imgui.same_line()end
            if imgui.button(label..' scale +0.01')then p.scale=math.min(3,p.scale+.01);editedControl() end
        end
    end
    local function header(label)
        uiSection()
        return not imgui.collapsing_header or imgui.collapsing_header(label)
    end
    if header('1. Aircraft Status')and WS.groups.info then
        imgui.text('Plane icon, damage, weapons and ammunition.')
        controls('Aircraft status',WS.groups.info)
    end
    if header('2. Radar')and WS.groups.radar then controls('Radar',WS.groups.radar)end
    if header('3. Mission Information')then
        imgui.text('Move and scale score, current target and time remaining together.')
        controls('Mission information',WS.simple.mission,false,{'score','target','time'})
        if imgui.indent then imgui.indent()end
        for _,pair in ipairs({{'score','Score'},{'target','Current target'},{'time','Time remaining'}})do
            if header(pair[2]..'##mission-child')then controls(pair[2],WS.simple[pair[1]],true)end
        end
        if imgui.unindent then imgui.unindent()end
    end
    if header('4. Center Ladder')and WS.groups.ladder then
        imgui.text('Pitch, speed, altitude and FPM move/scale together. Native FPM flight motion is retained.')
        controls('Center ladder',WS.groups.ladder)
        if imgui.indent then imgui.indent()end
        if header('Flight Path Marker alignment')then
            imgui.text('Fine alignment relative to ladder. Baseline X 0, Y 0. Negative Y moves up.')
            controls('FPM',WS.simple.fpm,true)
        end
        if imgui.unindent then imgui.unindent()end
    end
    if header('5. Wingman Status')then
        imgui.text('Additional offset from its normal position above the radar.')
        controls('Wingman status',WS.simple.wingman,false)
    end
    if header('6. Wingman Orders')then controls('Wingman orders',WS.simple.orders,true)end
    if header('7. Comms Portrait')then controls('Comms portrait',WS.simple.portrait,true)end
    if header('8. Radio messages')then controls('Radio messages',WS.simple.radio,true)end
    if header('Weapon projection calibration')then
        local w=simpleHUD.weapon
        imgui.text('Visual calibration only. Save with the HUD layout for this plane or as a global default.')
        local c,v=imgui.slider_float('SAAM circle size',w.saamScale,.25,2)
        if c then w.saamScale=v;ws_ui_dirty=true end
        if imgui.button('SAAM size -0.01')then w.saamScale=math.max(.25,w.saamScale-.01);ws_ui_dirty=true end
        if imgui.same_line then imgui.same_line()end
        if imgui.button('SAAM size +0.01')then w.saamScale=math.min(2,w.saamScale+.01);ws_ui_dirty=true end
        if imgui.button('Reset SAAM size')then w.saamScale=1;ws_ui_dirty=true end
        local c,v=imgui.slider_float('Bomb projection FOV adjustment (degrees)',w.bombFov,-40,40)
        if c then w.bombFov=v;ws_ui_dirty=true end
        if imgui.button('Bomb FOV -0.25')then w.bombFov=math.max(-40,w.bombFov-.25);ws_ui_dirty=true end
        if imgui.same_line then imgui.same_line()end
        if imgui.button('Bomb FOV +0.25')then w.bombFov=math.min(40,w.bombFov+.25);ws_ui_dirty=true end
        local c,v=imgui.slider_float('Bomb landing height offset',w.bombY,-4000,4000)
        if c then w.bombY=v;ws_ui_dirty=true end
        if imgui.button('Bomb height -1')then w.bombY=math.max(-4000,w.bombY-1);ws_ui_dirty=true end
        if imgui.same_line then imgui.same_line()end
        if imgui.button('Bomb height +1')then w.bombY=math.min(4000,w.bombY+1);ws_ui_dirty=true end
        if imgui.button('Reset bomb calibration')then w.bombFov=12.5;w.bombY=0;ws_ui_dirty=true end
    end
    if edited then
        ws_ui_sync();ws_ui_message=''
        if ws_simple_missing and ws_simple_missing()then WS.simpleNextFind=0 end
    end
    if ws_ui_dirty then imgui.text('Unsaved HUD changes')end
    if ws_ui_message~=''then imgui.text(ws_ui_message)end
end



local function ws_simple_discover(rows,rootPath)
    local entries,index,scoreParents={},{},{}
    local function add(o,keys)
        local n=full_name(o);if not n then return end
        if index[n]then return end
        local entry={o=o,k=n,fn=n,keys=keys,navigation=keys[1]=='mission'}
        entries[#entries+1]=entry;index[n]=entry
    end
    for _,o in ipairs(rows or{})do
        local n=full_name(o);local path=n and n:match('^%S+ (.+)$')
        if path and rootPath and path:sub(1,#rootPath+1)==rootPath..'.'then
            local name=leaf(o)
            local campaign=path:find('WBP_HUD_SubWidgets_Parts_CampaignInfo_000_C',1,true)
            local message=path:find('.HUDMessageWidget.',1,true)
            if message and name=='CanvasPanel_ComPortrait'then add(o,{'portrait'})
            elseif message and name=='WBP_HUD_SubWidgets_Parts_RadioText_000'then add(o,{'radio'})
            elseif name=='WBP_HUD_Command_Wingman_000'then add(o,{'orders'})
            elseif campaign and name:match('^WBP_HUD_SubWidgets_Parts_CampaignInfo_000_C')then simpleHUD.navigationMission=o;add(o,{'mission'})
            elseif campaign and name=='TimerAndGauge'then add(o,{'time'})
            elseif message and(name=='BonusScorePanel'or name=='BonusNamePanel')then add(o,{'mission','score'})
            elseif campaign and(name=='PrivateScore'or name=='PrivateScoreText'or name=='BorderScoreText'or
                name=='ScoreText'or name=='TeamScoreTitleText'or name=='VictoryScoreText')then
                local parent=call(o,'GetParent')
                if parent and(full_name(parent)or''):match('^HorizontalBox ')then scoreParents[full_name(parent)]=parent
                else scoreParents[n]=o end
            end
        end
    end
    -- Select each physical score row once, even when labels and digits share it.
    for _,o in pairs(scoreParents)do
        local parent=call(o,'GetParent');local nested=false
        for _=1,24 do
            if not parent then break end
            if scoreParents[full_name(parent)]then nested=true;break end
            parent=call(parent,'GetParent')
        end
        if not nested then add(o,{'score'})end
    end
    WS.simpleEntries,WS.simpleIndex=entries,index
end
local function ws_simple_delta(entry)
    local key=entry.keys[1]
    local d=simpleHUD.renderOffset(entry.o,entry.keys,not entry.navigation and(key=='score'or key=='time'))
    -- Child score/time/target rows inherit the campaign container's scale.
    -- Separate bonus-score panels carry their own mission key and scale once.
    if d then d.scale=key=='mission'and simpleHUD.scale('mission')or 1 end
    if not d then WS.states['simple:'..entry.keys[1]]='Waiting for parent geometry'end
    return d
end
ws_simple_missing=function()
    local found={}
    for _,e in ipairs(WS.simpleEntries or{})do
        if AC8Runtime.isLiveObject(e.o)then for _,key in ipairs(e.keys)do found[key]=true end end
    end
    if simpleHUD.navigationActive()and not found.mission then return true end
    for _,key in ipairs({'portrait','radio','orders','mission','score','time'})do
        local p=simpleHUD.values[key]
        if p and(p.x~=0 or p.y~=0 or(p.scale or 1)~=1)and not found[key]then return true end
    end
    return false
end

local function ws_identify()
    local selected_root = nil
    WS.canvasRoot=nil
    if UW then
        local ok, arr = pcall(function() return AC8Runtime.inventory.objects(UW,false) end)
        if ok and type(arr)=="table" then
            for _, w in ipairs(arr) do
                local nm=full_name(w)
                if type(nm)=="string" and nm:find("/Engine/Transient",1,true)
                    and nm:find(SCOPE,1,true) and not nm:find(".WidgetTree",1,true)
                    and call(w,"IsInViewport")==true then
                    selected_root=nm:match("%s(.+)$");WS.canvasRoot=w; break
                end
            end
        end
    end
    local entries, catalog, seen = {}, {}, {}
    if not selected_root then WS.entries={};WS.catalog={};WS.simpleEntries={};WS.simpleIndex={};return end
    
    local excluded = {}
    for _, t in ipairs(TARGETS) do
        if WS_GROUPS[t.key] and t.items then
            for _, e in ipairs(t.items) do
                excluded[full_name(e.o)]=true
            end
        end
    end
    local skip={BaseCanvas=true,MainCanvas=true,DynamicDrawCanvas=true,WorldToScreenDrawCanvas=true,
        LowestPriorityCanvasWorldToScreenDraw=true,LowestPriorityDynamicDrawCanvas=true,TopPriorityCanvas=true}
    for _, cls in ipairs(SEARCH_CLASSES) do
        local ok, arr=pcall(function() return AC8Runtime.inventory.objects(cls,false) end)
        if ok and type(arr)=="table" then
            for _, o in ipairs(arr) do
                local nm=full_name(o)
                local path=type(nm)=="string" and nm:match("%s(.+)$") or nil
                if path and path:sub(1,#selected_root+1)==selected_root.."." and not excluded[nm] then
                    local name=leaf(o)
                    local id=path:sub(#selected_root+2):gsub("WidgetTree_%d+","WidgetTree"):gsub("_C_%d+","_C")
                    local targetTest=name=="WBP_LocatorWidget" or name=="WBP_TargetSelectionDraw"
                    if not skip[name] and not seen[id] and (targetTest or (not name:find("Target") and not name:find("Locator") and not name:find("Seeker") and not name:find("Reticle"))) then
                        seen[id]=true
                        local category="Other flight HUD"
                        if path:find("ChroniclePersistent",1,true) then category="Central instruments"
                        elseif path:find("HUDMessageWidget",1,true) then category="Comms and messages"
                        elseif path:find("MiniMap",1,true) then category="Radar"
                        elseif path:find("PlayerState",1,true) then category="Aircraft and weapons" end
                        local vis=call(o,"GetVisibility")
                        local sx,sy=get_rt_scale(o);local x,y=get_rt_xy(o)
                        if targetTest then category="Target scale tests" end
                        entries[id]={o=o,k=nm,fn=nm,scaleOnly=targetTest}
                        catalog[#catalog+1]={id=id,name=name,category=category,path=id,
                            visibility=vis or -1,x=x or 0,y=y or 0,scaleX=sx or 1,scaleY=sy or 1,scaleOnly=targetTest,}
                    end
                end
            end
        end
    end

    local scoreClass=api:find_uobject("Class /Script/UMG.Widget")
    if scoreClass then
        local ok,arr=pcall(function() return AC8Runtime.inventory.objects(scoreClass,false) end)
        if ok and type(arr)=="table" then
            ws_simple_discover(arr,selected_root)
            for _,o in ipairs(arr) do
                local nm=full_name(o)
                local path=type(nm)=="string" and nm:match("%s(.+)$") or nil
                local name=leaf(o)
                local radioLabel=path and path:find('WBP_HUD_SubWidgets_Parts_RadioText_000',1,true)
                    and (name=='SpeakerNameText' or name=='SubTitleMessageText')
                if path and type(name)=="string" and (name:lower():find("score",1,true) or radioLabel)
                    and path:sub(1,#selected_root+1)==selected_root.."." and not excluded[nm] then
                    local id=path:sub(#selected_root+2):gsub("WidgetTree_%d+","WidgetTree"):gsub("_C_%d+","_C")
                    if not seen[id] then
                        seen[id]=true
                        local sx,sy=get_rt_scale(o);local x,y=get_rt_xy(o)
                        entries[id]={o=o,k=nm,fn=nm}
                        catalog[#catalog+1]={id=id,name=name,category=radioLabel and "Radio text labels" or "Score readouts",path=id,
                            visibility=call(o,"GetVisibility") or -1,x=x or 0,y=y or 0,
                            scaleX=sx or 1,scaleY=sy or 1,}
                    end
                end
            end
        end
    end
    table.sort(catalog,function(a,b) return a.category..a.name < b.category..b.name end)
    WS.entries,WS.catalog=entries,catalog
end
local function ws_transform(entry,p,tag,delta)
    local o,k=entry.o,entry.k
    delta=delta or{X=0,Y=0}
    local navigation=entry.navigation
    local groupScale=delta.scale or 1
    local owns=p.enabled or delta.X~=0 or delta.Y~=0 or groupScale~=1 or(navigation and simpleHUD.navigationActive())
    local base=WS.originals[k]
    if not owns and not base then WS.states[tag]="Unchanged";return end
    local ok,err=pcall(function()
        if not base then
            local x,y,sx,sy=get_rt_all(o)
            assert(x and y and sx and sy,"Original transform unavailable")
            base={x=x,y=y,sx=sx,sy=sy};WS.originals[k]=base
        end
        local x=base.x+(p.enabled and p.x or 0)+delta.X
        local y=base.y+(p.enabled and p.y or 0)+delta.Y
        local scale=(p.enabled and p.scale or 1)*groupScale
        local wantSX,wantSY=base.sx*scale,base.sy*scale
        if navigation then
            local nx,ny,nsx,nsy=simpleHUD.navigationPose(o,x,y,wantSX,wantSY)
            if not nx then WS.states[tag]='Waiting for navigation group geometry';return end
            x,y,wantSX,wantSY=nx,ny,nsx,nsy
        end
        local cx,cy,sx,sy=get_rt_all(o);local wrote=false
        if not entry.scaleOnly and (not cx or math.abs(cx-x)>0.01 or not cy or math.abs(cy-y)>0.01) then
            o:SetRenderTranslation({X=x,Y=y});wrote=true
        end
        if not sx or math.abs(sx-wantSX)>0.001 or not sy or math.abs(sy-wantSY)>0.001 then
            o:SetRenderScale({X=wantSX,Y=wantSY});wrote=true
        end
        if wrote then cx,cy,sx,sy=get_rt_all(o) end
        assert(cx and cy and sx and sy and (entry.scaleOnly or (math.abs(cx-x)<1 and math.abs(cy-y)<1))
            and math.abs(sx-wantSX)<0.01 and math.abs(sy-wantSY)<0.01,"Transform read-back differs")
        if not owns then WS.originals[k]=nil end
    end)
    WS.states[tag]=ok and (owns and "Write verified; check headset" or "Restored") or ("Error: "..tostring(err))
end

local function readField(o,k)return o[k]end
local function ws_field(o,k)
    local ok,v=pcall(readField,o,k)
    if ok then return v end
end
local function ws_slot_copy(slot)
    local offsets=ws_field(slot,"Offsets");local anchors=ws_field(slot,"Anchors")
    local alignment=ws_field(slot,"Alignment")
    if not offsets or not anchors or not alignment then return nil end
    local min=ws_field(anchors,"Minimum");local max=ws_field(anchors,"Maximum")
    if not min or not max then return nil end
    local out={Offsets={},Anchors={Minimum={},Maximum={}},Alignment={}}
    for _,k in ipairs({"Left","Top","Right","Bottom"}) do
        local v=tonumber(ws_field(offsets,k));if not v then return nil end;out.Offsets[k]=v
    end
    for _,k in ipairs({"X","Y"}) do
        local a=tonumber(ws_field(min,k));local b=tonumber(ws_field(max,k));local c=tonumber(ws_field(alignment,k))
        if not a or not b or not c then return nil end
        out.Anchors.Minimum[k]=a;out.Anchors.Maximum[k]=b;out.Alignment[k]=c
    end
    out.ZOrder=tonumber(ws_field(slot,"ZOrder")) or 0
    local auto=ws_field(slot,"bAutoRemoveOnWorldRemoved")
    if type(auto)~="boolean" then return nil end
    out.bAutoRemoveOnWorldRemoved=auto
    return out
end
local function ws_canvas_capture(root)
    local key=full_name(root)
    if WS.canvasBase and WS.canvasBase.key==key then return WS.canvasBase end
    local b={key=key,clipping=call(root,"GetClipping")}
    local cls=api:find_uobject("Class /Script/UMG.GameViewportSubsystem")
    if cls then
        local ok,arr=pcall(function() return AC8Runtime.inventory.objects(cls,false) end)
        if ok and type(arr)=="table" then
            for _,sub in ipairs(arr) do
                if call(sub,"IsWidgetAdded",root)==true then
                    local slot=call(sub,"GetWidgetSlot",root)
                    local copy=slot and ws_slot_copy(slot)
                    if copy then b.slot=copy;b.subsystem=sub;break end
                end
            end
        end
    end
    WS.canvasBase=b;return b
end
local function ws_canvas_apply()
    local p=WS.canvas;local root=WS.canvasRoot
    if not p then return end
    if not root then WS.states.canvas="MainFlight canvas not found";return end
    if not p.enabled and not WS.canvasBase then WS.states.canvas="Canvas unchanged";return end
    local ok,err=pcall(function()
        local b=ws_canvas_capture(root)
        ws_transform({o=root,k=b.key},p,"canvas")
        assert(not WS.states.canvas or WS.states.canvas:sub(1,6)~="Error:",WS.states.canvas)
        local extraW=p.enabled and p.extraWidth or 0
        local extraH=p.enabled and p.extraHeight or 0
        if b.slot then
            local target=ws_slot_copy(b.slot)
            local a=target.Anchors
            target.Offsets.Right=target.Offsets.Right+extraW*(a.Maximum.X~=a.Minimum.X and -1 or 1)
            target.Offsets.Bottom=target.Offsets.Bottom+extraH*(a.Maximum.Y~=a.Minimum.Y and -1 or 1)
            local current=call(b.subsystem,"GetWidgetSlot",root)
            local c=current and ws_slot_copy(current)
            assert(c,"Viewport slot read-back unavailable")
            if math.abs(c.Offsets.Right-target.Offsets.Right)>.01 or math.abs(c.Offsets.Bottom-target.Offsets.Bottom)>.01 then
                b.subsystem:SetWidgetSlot(root,target)
                b.resized=true
            end
            c=ws_slot_copy(call(b.subsystem,"GetWidgetSlot",root))
            assert(c and math.abs(c.Offsets.Right-target.Offsets.Right)<1 and math.abs(c.Offsets.Bottom-target.Offsets.Bottom)<1,"Viewport size read-back differs")
        elseif extraW~=0 or extraH~=0 then
            WS.states.canvas="Transform applied; layout expansion unavailable: viewport slot could not be captured"
        end
        if p.enabled and p.inheritClip then
            assert(type(b.clipping)=="number","Original root clipping unavailable")
            root:SetClipping(0);b.clipChanged=true
            assert(call(root,"GetClipping")==0,"Root clipping read-back differs")
        elseif b.clipChanged then root:SetClipping(b.clipping);b.clipChanged=false end
        if not p.enabled then WS.states.canvas="Canvas baseline restored" end
    end)
    if not ok then WS.states.canvas="Canvas error: "..tostring(err) end
end

local function ws_ancestor_conflict(id)

    for other,p in pairs(WS.components) do
        if other~=id and p.enabled and id:sub(1,#other+1)==other.."." then return true end
    end
    return false
end
workshop_after=function()
    if WS.elapsed%20==0 then pcall(ws_canvas_apply) end
    for _, t in ipairs(TARGETS) do
        local p=WS.groups[t.key]
        if p and WS_GROUPS[t.key] then
            local tag="group:"..t.key
            if not t.items or #t.items==0 then WS.states[tag]="Not found in this mission"
            elseif not WS.groupbase[t.key].active then
                for _,e in ipairs(t.items) do
                    ws_transform({o=e.o,k=full_name(e.o),navigation=t.key=='radar'},p,tag)
                end
            end
        end
    end
    local handled={}
    for id,p in pairs(WS.components)do
        local tag='component:'..id;local entry=WS.entries[id]
        local group=entry and WS.simpleIndex and WS.simpleIndex[entry.k]
        local delta=group and ws_simple_delta(group)or(not group and{X=0,Y=0})
        if group then handled[group.k]=true end
        if not entry then WS.states[tag]='Not found in this mission'
        elseif not delta then WS.states[tag]='Waiting for group geometry'
        elseif p.enabled and ws_ancestor_conflict(id)then
            ws_transform(entry,{enabled=false,x=0,y=0,scale=1},tag,delta)
            WS.states[tag]='Disable the parent component first'
        else ws_transform(entry,p,tag,delta)end
    end
    for _,entry in ipairs(WS.simpleEntries or{})do
        if not handled[entry.k]and AC8Runtime.isLiveObject(entry.o)then
            local delta=ws_simple_delta(entry)
            if delta then ws_transform(entry,{enabled=false,x=0,y=0,scale=1},'simple:'..entry.keys[1],delta)end
        end
    end
    if simpleHUD.applyWingman then simpleHUD.applyWingman()end
end
workshop_poll=function()
    if WS.hudEpoch~=AC8Runtime.context.epoch then
        WS.hudEpoch=AC8Runtime.context.epoch;WS.elapsed=19;WS.needScan=true;WS.acquireUntil=AC8Runtime.tick+120
    end
    if not gameplay(AC8Runtime)then return end
    WS.elapsed=WS.elapsed+1;WS.scan_elapsed=WS.scan_elapsed+1
    if WS.elapsed%20~=0 then return end
    local ok,cmd=pcall(function() return simpleHUD.layoutCommand end)
    if ok and type(cmd)=="table" and type(cmd.revision)=="string" and cmd.revision~=WS.revision
        and type(cmd.groups)=="table" and type(cmd.components)=="table" then
        local valid=true;local n=0
        for k in pairs(WS_GROUPS) do if not ws_params(cmd.groups[k]) then valid=false end end
        for id,p in pairs(cmd.components) do
            n=n+1;if type(id)~="string" or not ws_params(p) or n>24 then valid=false end
        end
        if cmd.canvas then
            local c=cmd.canvas
            valid=valid and ws_params(c) and type(c.extraWidth)=="number" and c.extraWidth>=0 and c.extraWidth<=4000
                and type(c.extraHeight)=="number" and c.extraHeight>=0 and c.extraHeight<=4000 and type(c.inheritClip)=="boolean"
        end
        if valid then
            WS.canvas=cmd.canvas or {enabled=false,x=0,y=0,scale=1,extraWidth=0,extraHeight=0,inheritClip=false}
            
            for id in pairs(WS.components) do
                if not cmd.components[id] then cmd.components[id]={enabled=false,x=0,y=0,scale=1} end
            end
            workshop_active=true
            WS.groups,WS.components=cmd.groups,cmd.components
            WS.simple=simpleHUD.normalize(cmd.simpleGroups);simpleHUD.values=WS.simple
            simpleHUD.weapon=simpleHUD.weaponValues(cmd.weaponCalibration)
            simpleHUD.targetBase=simpleHUD.targetCalibration(cmd.simpleTargetBase)
            for _,t in ipairs(TARGETS) do
                local p=WS.groups[t.key];local b=WS.groupbase[t.key]
                if p and b and b.active then
                    t.x=b.x+(p.enabled and p.x or 0);t.y=b.y+(p.enabled and p.y or 0)
                    t.sc=b.sc*(p.enabled and p.scale or 1);t.active=b.active
                    t.scApplied=nil
                end
            end
            WS.revision=cmd.revision
            if cmd.rescan~=WS.rescan then WS.rescan=cmd.rescan;WS.needScan=true end
        end
    end
    local missing=#WS.catalog==0
    for id,p in pairs(WS.components)do if p.enabled and not WS.entries[id]then missing=true;break end end
    if WS.needScan or (missing and AC8Runtime.tick<(WS.acquireUntil or 0) and WS.scan_elapsed>=20) then
        WS.needScan=false
        WS.scan_elapsed=0
        pcall(rescan_all)
        pcall(ws_identify)
    end

    -- Only an adjusted, missing group requests a fresh current-owner walk.
    -- No global enumeration or geometry cache is added to settled frames.
    if ws_simple_missing()and AC8Runtime.tick>=(WS.simpleNextFind or 0)then
        local rows=AC8Runtime.ownedWidgets and AC8Runtime.ownedWidgets(AC8Runtime.context.root)
        local rootPath=(AC8Runtime.context.rootName or''):match('^%S+ (.+)$')
        if rows and rootPath then ws_simple_discover(rows,rootPath)end
        WS.simpleRetryGap=math.min((WS.simpleRetryGap or 60)*2,600)
        WS.simpleNextFind=AC8Runtime.tick+WS.simpleRetryGap
    end
    pcall(apply)
end
pcall(ws_identify)
pcall(function()uevr.sdk.callbacks.on_script_reset(function()
    for _,entry in ipairs(WS.simpleEntries or{})do
        local base=WS.originals[entry.k]
        if base and AC8Runtime.isLiveObject(entry.o)and full_name(entry.o)==entry.k then
            call(entry.o,'SetRenderTranslation',{X=base.x,Y=base.y})
            call(entry.o,'SetRenderScale',{X=base.sx,Y=base.sy})
        end
    end
    for _,t in ipairs(TARGETS)do
        if t.key=='radar'then for _,entry in ipairs(t.items or{})do
            local name=full_name(entry.o);local base=name and WS.originals[name]
            if base and AC8Runtime.isLiveObject(entry.o)then
                call(entry.o,'SetRenderTranslation',{X=base.x,Y=base.y})
                call(entry.o,'SetRenderScale',{X=base.sx,Y=base.sy})
            end
        end end
    end
    simpleHUD.values=simpleHUD.normalize()
end)end)

local layoutEpoch=-1
layout_scope_sync=function()
 local context=AC8Runtime.context
 updateHUDProfiles()
 if layoutEpoch~=context.epoch then
  layoutEpoch=context.epoch
  -- Forget old-world pointers without trying to restore destroyed widgets.
  for _,t in ipairs(TARGETS)do t.objs=nil;t.items=nil;t.applied=false;t.opApplied=nil;t.scApplied=nil end
  simpleHUD.navigationBases=setmetatable({},{__mode='k'});WS.entries={};WS.originals={};WS.catalog={};WS.canvasRoot=nil;WS.canvasBase=nil
  WS.simpleEntries={};WS.simpleIndex={};WS.simpleNextFind=0;WS.simpleRetryGap=60
  WS.needScan=true;WS.elapsed=19;WS.scan_elapsed=20;WS.acquireUntil=AC8Runtime.tick+120
  if gameplay(AC8Runtime)then pcall(resolve_all,false)end
 end
 return gameplay(AC8Runtime)
end
local ticks = 0
pcall(function()
    uevr.sdk.callbacks.on_post_engine_tick(function()
        if not layout_scope_sync()then return end
        if workshop_poll then pcall(workshop_poll) end
        lt14_apply("post_engine_tick")
        ticks = ticks + 1

    end)
end)

end)(AC8Runtime.host('layout','AC8_VR-LadderFixV19Fast.lua'),AC8Runtime.json,AC8Runtime.log)
end
end


end

-- EDITABLE HUD PRESENTATION DEFAULTS (UEVR UI settings, not world/camera scale).
-- Applied only on first setup of this profile, tracked by a persistent marker.
-- Later launches/reloads preserve the user's saved UEVR settings.
-- Set enabled=false to skip first setup. Use UEVR Save Config for later edits.
-- depth = UI distance in metres; scale = UI size multiplier.
-- curvature = cylinder angle in degrees. Applying this also selects curved UI.
-- Validation bounds: depth/scale 0.01..100, curvature 1..359. No save_config call.
local HUD_PRESENTATION = {
    enabled = true,
    depth = 1.0,
    scale = 1.0,
    curvature = 113.625,
}

assert(not registry.sections['hud_layout'], 'Duplicate AC8 VR section: hud_layout')
registry.sections['hud_layout'] = function(R)
-- Own final VR HUD positioning and ordered inherited layout services.
-- A single startup transaction; never enforce these settings every frame.
-- Defer while cinematic restoration owns the UI, so it cannot undo the preset.
local presentation={done=false,status='waiting'}
R.hudPresentationDefaults=presentation
local setupFile='AC8_VR-HUDPresentationSetup.json'
local setupJSON=rawget(_G,'json')
local checked,previous=pcall(function()return setupJSON.load_file(setupFile)end)
if not checked then
 presentation.done=true;presentation.status='setup marker unavailable; preserving UEVR settings'
elseif previous~=nil then
 presentation.done=true
 presentation.status=type(previous)=='table'and previous.version==1 and previous.initialized==true
  and'previously initialized; saved UEVR settings preserved'or'invalid setup marker; preserving UEVR settings'
end
local function applyPresentationDefaults()
 if presentation.done then return end
 if HUD_PRESENTATION.enabled==false then presentation.done=true;presentation.status='disabled';return end
 local settings={
  {key='UI_Distance',value=HUD_PRESENTATION.depth,min=.01,max=100},
  {key='UI_Size',value=HUD_PRESENTATION.scale,min=.01,max=100},
  {key='UI_Cylinder_Angle',value=HUD_PRESENTATION.curvature,min=1,max=359},
  {key='UI_OverlayType',value=1,min=1,max=1},
 }
 for _,setting in ipairs(settings)do
  local v=setting.value
  if type(v)~='number'or v~=v or v<setting.min or v>setting.max then
   presentation.done=true;presentation.status='invalid '..setting.key
   R.errors.hudPresentationDefaults=presentation.status;return
  end
 end
 local ci=rawget(_G,'AC8_CinematicIntent')
 if ci and(ci.maskActive or ci.hangarActive or ci.recoveringUI or ci.guardForced or
    (ci.saved and next(ci.saved)~=nil)or(ci.hangarSaved and next(ci.hangarSaved)~=nil))then return end
 local function read(key)
  local ok,v=pcall(function()return uevr.params.vr:get_mod_value(key)end)
  local n=ok and tonumber(v)
  if n and n==n and math.abs(n)<1e9 then return n end
 end
 local before={}
 for _,setting in ipairs(settings)do
  local value=read(setting.key)
  if value==nil then presentation.status='waiting for UEVR UI settings';return end
  before[setting.key]=value
 end
 -- Once a write is attempted, never retry later over a user's menu adjustment.
 presentation.done=true
 -- Persist admission before touching UEVR. Reloads cannot repeat a partial setup.
 -- A failed/unverifiable marker write leaves all user settings untouched.
 local marked,marker=pcall(function()
  setupJSON.dump_file(setupFile,{version=1,initialized=true},2)
  return setupJSON.load_file(setupFile)
 end)
 if not marked or type(marker)~='table'or marker.version~=1 or marker.initialized~=true then
  presentation.status='could not persist first setup; preserving UEVR settings'
  R.errors.hudPresentationDefaults=presentation.status;return
 end
 local changed={}
 for _,setting in ipairs(settings)do
  if math.abs(before[setting.key]-setting.value)>.0001 then
   changed[#changed+1]=setting.key
   local ok=pcall(function()uevr.params.vr.set_mod_value(setting.key,tostring(setting.value))end)
   local actual=read(setting.key)
   if not ok or not actual or math.abs(actual-setting.value)>.0001 then
    for i=#changed,1,-1 do
     local key=changed[i];pcall(function()uevr.params.vr.set_mod_value(key,tostring(before[key]))end)
    end
    presentation.status='UEVR write failed; previous values restored where possible'
    R.errors.hudPresentationDefaults=presentation.status;return
   end
  end
 end
 presentation.status='first setup complete; saved UEVR settings take priority on future launches'
end
applyPresentationDefaults()
R.host('layout','HUD presentation defaults').sdk.callbacks.on_post_engine_tick(applyPresentationDefaults)

AC8Runtime.layoutEnabled=true
for i,setup in ipairs(AC8Runtime.inheritedSetup)do local ok,err=pcall(setup);if not ok then AC8Runtime.errors['inherited layout '..i]=tostring(err)end end
AC8Runtime.inheritedSetup={}

do
-- ac8 campaign target row
(function(uevr,json,print)


local api=uevr.api
local old=rawget(_G,'AC8_CampaignTargetRow')
if old then old.stopped=true;if old.restore then old.restore() end end
local S={version=2,ticks=0,enabled=true,stopped=false,status='waiting for flight HUD'}
rawset(_G,'AC8_CampaignTargetRow',S)
local function invokeInventory(o,k,...)if AC8Runtime.inventory then return AC8Runtime.inventory.call(o,k,...)end;return o[k](o,...)end
local function call(o,k,...)
    if o~=uevr.api and not AC8Runtime.isLiveObject(o)then return nil end
    if not o then return nil end
    local ok,v=pcall(invokeInventory,o,k,...);if ok then return v end
end
local function readField(o,k)return o[k]end
local function field(o,k) local ok,v=pcall(readField,o,k);if ok then return v end end
local function full(o) return call(o,'get_full_name') end
local function xy(v)
    local x=field(v,'X') or field(v,'x');local y=field(v,'Y') or field(v,'y')
    if type(x)=='number' and type(y)=='number' and x==x and y==y then return {X=x,Y=y} end
end
local function transform(o)
    if not AC8Runtime.isLiveObject(o)then return nil,nil end
    local r=call(o,'get_property','RenderTransform')
    return xy(field(r,'Translation')),xy(field(r,'Scale'))
end
function S.restore()
    if S.base and S.row and full(S.row)==S.base.name then
        pcall(function() S.row:SetRenderTranslation(S.base.translation);S.row:SetRenderScale(S.base.scale) end)
    end
    S.base=nil;S.row=nil;S.widgets=nil
end
local function discard()S.base=nil;S.row=nil;S.widgets=nil end
local function discover(forceGlobal)
    local found={}
    local R=AC8Runtime
    local owned=not forceGlobal and R.ownedWidgets and R.ownedWidgets(R.context.root)
    local candidates=owned
    if not candidates then
        candidates=call(call(api,'find_uobject','Class /Script/UMG.Widget'),'get_objects_matching',false)or{}
    end
    for _,w in ipairs(candidates)do
        local n=full(w) or '';local key=n:match('%.([^%.]+)$')
        if n:find('/Engine/Transient.',1,true) and n:find('WBP_HUD_Chronicle_MainFlight_000_C_',1,true)
            and n:find('WBP_HUD_SubWidgets_Parts_CampaignInfo_000_C_',1,true)
            and (key=='TargetText' or key=='TargetInfoText' or key=='PrivateScore' or key=='TimeText') then
            
            local rootPath=n:match('^[^ ]+ (.-WBP_HUD_Chronicle_MainFlight_000_C_%d+)%.')
            local root=rootPath and call(api,'find_uobject','WBP_HUD_Chronicle_MainFlight_000_C '..rootPath)
            if call(root,'IsInViewport')==true then
                found[rootPath]=found[rootPath] or {};found[rootPath][key]=w
            end
        end
    end
    local matches={}
    for _,w in pairs(found) do
        local row=call(w.TargetText,'GetParent')
        if row and full(row)==full(call(w.TargetInfoText,'GetParent')) and w.PrivateScore and w.TimeText then
            matches[#matches+1]={widgets=w,row=row}
        end
    end
    if #matches~=1 then if owned then return discover(true)end;S.status='waiting for one active campaign target row';return end
    S.widgets=matches[1].widgets;S.row=matches[1].row
    local t,sc=transform(S.row)
    if not t or not sc then S.row=nil;S.widgets=nil;S.status='row transform unavailable';return end
    S.base={name=full(S.row),translation=t,scale=sc}
end
local function apply()
    if S.stopped or not gameplay(AC8Runtime)then return end
    S.ticks=S.ticks+1
    
    local hud=full(call(call(api,'get_player_controller',0),'GetHUD')) or ''
    if not hud:match('^LiveMainHUDParent4K_C ')then discard();return end
    if not S.enabled then S.restore();return end
    if S.hud~=hud then discard();S.hud=hud end
    if S.row and full(S.row)~=S.base.name then S.restore() end
    if not S.row then
        if S.ticks%4~=1 then return end
        discover();if not S.row then return end
    end
    -- Keep the game's internal row layout. Parent movement is owned by the
    -- existing mission group; this owner applies only the target subgroup.
    local delta=simpleHUD.renderOffset(S.row,{'target'},true)
    local current,sc=transform(S.row)
    if not delta or not current or not sc then return end
    local wanted={X=S.base.translation.X+delta.X,Y=S.base.translation.Y+delta.Y}
    if math.abs(current.X-wanted.X)>.01 or math.abs(current.Y-wanted.Y)>.01 then S.row:SetRenderTranslation(wanted)end
    if math.abs(sc.X-S.base.scale.X)>.001 or math.abs(sc.Y-S.base.scale.Y)>.001 then S.row:SetRenderScale(S.base.scale)end
    S.status='native row arrangement with independent target controls'
end
uevr.sdk.callbacks.on_post_engine_tick(function()
    local ok,err=pcall(apply);if not ok then S.status=tostring(err) end
end)
pcall(function() uevr.sdk.callbacks.on_script_reset(S.restore) end)

end)(AC8Runtime.host('layout','AC8_VR-CampaignTargetRow.lua'),AC8Runtime.json,AC8Runtime.log)
end

do
-- ac8 velocity alignment
(function(uevr,json,print)


local api=uevr.api
local old=rawget(_G,'AC8_VelocityAlignment')
if old then old.stopped=true;if old.restore then old.restore() end end
local S={version=10,enabled=true,ticks=0,stopped=false,status='waiting for flight HUD',
    -- Identity container fallback; icon reference preserves confirmed 1080p pose.
    -- Current slot/pivot data still links the FPM container to the ladder.
    x=0,y=0,scale=1,iconX=1920,iconY=1080}
rawset(_G,'AC8_VelocityAlignment',S)
local function invokeInventory(o,k,...)if AC8Runtime.inventory then return AC8Runtime.inventory.call(o,k,...)end;return o[k](o,...)end
local function call(o,k,...)
    if o~=uevr.api and not AC8Runtime.isLiveObject(o)then return nil end
    if not o then return nil end
    local ok,v=pcall(invokeInventory,o,k,...);if ok then return v end
end
local function readField(o,k)return o[k]end
local function field(o,k)local ok,v=pcall(readField,o,k);if ok then return v end end
local function full(o)return call(o,'get_full_name')end
local function xy(v)
    local x,y=field(v,'X'),field(v,'Y')
    if type(x)=='number' and type(y)=='number' and x==x and y==y then return {X=x,Y=y} end
end
local function transform(o)
    if not AC8Runtime.isLiveObject(o)then return nil,nil end
    local r=call(o,'get_property','RenderTransform')
    return xy(field(r,'Translation')),xy(field(r,'Scale'))
end
local function matches(a,b,tolerance)
    return a and b and math.abs(a.X-b.X)<tolerance and math.abs(a.Y-b.Y)<tolerance
end
local function redraw()
    call(S.velocity,'InvalidateLayoutAndVolatility')
    local parent=call(S.velocity,'GetParent')
    for _=1,8 do
        if not parent then break end
        if (full(parent) or ''):match('^RetainerBox ') then call(parent,'RequestRender') end
        parent=call(parent,'GetParent')
    end
end
function S.restore()
    for _,r in ipairs(S.originals or {})do
        if full(r.object)==r.name then
            call(r.object,'SetRenderTranslation',r.translation);call(r.object,'SetRenderScale',r.scale)
        end
    end
    S.velocity=nil;S.icon=nil;S.altimeter=nil;S.root=nil;S.originals=nil;S.lastIconPin=nil
end
local function discard()S.velocity=nil;S.icon=nil;S.altimeter=nil;S.root=nil;S.originals=nil;S.lastIconPin=nil end
local function discover(forceGlobal)
    local sets={}
    local R=AC8Runtime
    local owned=not forceGlobal and R.ownedWidgets and R.ownedWidgets(R.context.root)
    local candidates=owned
    if not candidates then
        candidates=call(call(api,'find_uobject','Class /Script/UMG.Widget'),'get_objects_matching',false)or{}
    end
    for _,w in ipairs(candidates)do
        local n=full(w) or '';local leaf=n:match('%.([^%.]+)$')
        if (leaf=='Velocity' or leaf=='VelocityIcon') and n:find('.WBP_ChroniclePersistent.',1,true) then
            local path=n:match('^[^ ]+ (.-WBP_HUD_Chronicle_MainFlight_000_C_%d+)%.')
            local root=path and call(api,'find_uobject','WBP_HUD_Chronicle_MainFlight_000_C '..path)
            if call(root,'IsInViewport')==true then sets[path]=sets[path] or {root=root};sets[path][leaf]=w end
        end
    end
    local found={}
    for _,w in pairs(sets)do if w.Velocity and w.VelocityIcon then found[#found+1]=w end end
    if #found~=1 then if owned then return discover(true)end;return end
    local w=found[1];local originals={}
    for _,o in ipairs({w.Velocity,w.VelocityIcon})do
        local t,sc=transform(o);if not t or not sc then return end
        originals[#originals+1]={object=o,name=full(o),translation=t,scale=sc}
    end
    S.velocity=w.Velocity;S.icon=w.VelocityIcon;S.root=w.root;S.originals=originals
end
local function apply()
    if S.stopped or not gameplay(AC8Runtime)then return end
    S.ticks=S.ticks+1;if S.ticks%4~=0 then return end
    local hud=full(call(call(api,'get_player_controller',0),'GetHUD')) or ''
    if not hud:match('^LiveMainHUDParent4K_C ')then discard();return end
    if not S.enabled then S.restore();return end
    if S.hud~=hud then discard();S.hud=hud end
    if S.root and call(S.root,'IsInViewport')~=true then S.restore() end
    if not S.velocity then discover();if not S.velocity then return end end
    -- Only the container follows the ladder's presentation transform.
    -- The icon's native velocity animation remains untouched after initial pin.
    local x,y,size,sizeY,linked=S.x,S.y,S.scale,S.scale,false
    if simpleHUD and simpleHUD.fpmPose then x,y,size,sizeY,linked=simpleHUD.fpmPose(S.velocity,x,y,size)end
    local trim=simpleHUD and simpleHUD.values and simpleHUD.values.fpm
    if trim then x=x+trim.x*size;y=y+trim.y*(sizeY or size)end
    local wanted={X=x,Y=y};local scale={X=size,Y=sizeY or size}
    local translation,currentScale=transform(S.velocity);local changed=false
    if not matches(translation,wanted,.01) then call(S.velocity,'SetRenderTranslation',wanted);changed=true end
    if not matches(currentScale,scale,.001) then call(S.velocity,'SetRenderScale',scale);changed=true end

    -- Initialize once in the current slot/parent coordinate space. Defer until
    -- arranged instead of flashing the old fixed centre at another aspect.
    -- Later translation/scale belong to native flight motion; never re-pin it.
    if not S.lastIconPin then
        local iconTranslation,iconScale=transform(S.icon)
        local unit={X=1,Y=1}
        local ix,iy=simpleHUD.resetCenter(S.icon,S.iconX,S.iconY,unit)
        if ix~=nil and iy~=nil then
            local iconWanted={X=ix,Y=iy}
            if not matches(iconTranslation,iconWanted,.01) then call(S.icon,'SetRenderTranslation',iconWanted);changed=true end
            if not matches(iconScale,unit,.001) then call(S.icon,'SetRenderScale',unit);changed=true end
            S.lastIconPin=S.ticks
        end
    end
    if changed then redraw() end
    S.status=linked and'FPM centered on ladder; native motion retained'or'Calibrated FPM placement; waiting for shared canvas layout'

end
uevr.sdk.callbacks.on_post_engine_tick(function()
    local ok,err=pcall(apply);if not ok then S.status=tostring(err) end
    
end)
pcall(function()uevr.sdk.callbacks.on_script_reset(S.restore)end)

end)(AC8Runtime.host('layout','AC8_VR-VelocityAlignment.lua'),AC8Runtime.json,AC8Runtime.log)
end

do
-- forward locator
(function(uevr,json,print)
do

local old=rawget(_G,'AC8_ForwardLocator')
if old then old.stopped=true;if old.restore then old.restore() end end
-- Native locator conversion owns the coordinate correction. Preserve the
-- game-authored forward offset; do not stack the old -600 compensation.
local S={version=4,enabled=true,stopped=false,y=0,ticks=0,status='waiting for locator widget'}
rawset(_G,'AC8_ForwardLocator',S)
local function invokeInventory(o,k,...)if AC8Runtime.inventory then return AC8Runtime.inventory.call(o,k,...)end;return o[k](o,...)end
local function call(o,k,...)
    if o~=uevr.api and not AC8Runtime.isLiveObject(o)then return nil end
    if not o then return nil end
    local ok,v=pcall(invokeInventory,o,k,...);if ok then return v end
end
local function full(o)return call(o,'get_full_name')end
local function read(o)
    local ok,v=pcall(function()
        local p=o:get_property('FowardLocatorOffset')
        if type(p.X)~='number' or type(p.Y)~='number' then return nil end
        return {X=p.X,Y=p.Y}
    end)
    if ok then return v end
end
local function same(a,b)return a and b and math.abs(a.X-b.X)<.001 and math.abs(a.Y-b.Y)<.001 end
local function set(o,p)
    if same(read(o),p)then return true end
    pcall(function()local v=o:get_property('FowardLocatorOffset');v.X=p.X;v.Y=p.Y end)
    if not same(read(o),p) then pcall(function()o:set_property('FowardLocatorOffset',p)end) end
    return same(read(o),p)
end

function S.restore()
    if S.widget and full(S.widget)==S.name and S.original then
        local current=read(S.widget)
        if same(current,S.applied) and not set(S.widget,S.original) then return false end
    end
    S.widget=nil;S.name=nil;S.original=nil;S.applied=nil;S.verified=false
    return true
end
local function discard()S.widget=nil;S.name=nil;S.original=nil;S.applied=nil;S.verified=false end
local function apply()
    if S.stopped then return end
    S.ticks=S.ticks+1
    local pc=call(uevr.api,'get_player_controller',0);local hud=call(pc,'GetHUD');local hn=full(hud) or ''
    local ci=rawget(_G,'AC8_CinematicIntent')
    if not hn:match('^LiveMainHUDParent4K_C ')then discard();S.status='inactive';return end
    if not S.enabled or (ci and ci.cameraLocked) then
        S.restore();S.status='inactive';return
    end
    local canvas=call(hud,'get_property','DynamicCanvas')
    local cn=full(canvas)
    if cn~=S.canvasName then discard();S.canvasName=cn end
    if S.widget and full(S.widget)~=S.name then discard()end
    if not S.widget then
        for i=0,math.min(call(canvas,'GetChildrenCount') or 0,64)-1 do
            local w=call(canvas,'GetChildAt',i);local n=full(w) or ''
            if n:match('^WBP_HUD_DynamicWidget_Parts_Locator_000_C ') and n:match('%.WBP_LocatorWidget$') then
                local initial=read(w)
                if initial then S.widget=w;S.name=n;S.original=initial;break end
            end
        end
    end
    if not S.widget then S.status='locator widget unavailable';return end
    local wanted={X=S.original.X,Y=S.original.Y+S.y}
    S.verified=set(S.widget,wanted)
    if S.verified then S.applied=wanted;S.status='forward arrow correction applied; user confirmed position'
    else
        S.enabled=false
        local restored=set(S.widget,S.original)
        S.status=restored and 'write unavailable; original restored' or 'write unavailable; restoration needs checking'
        if restored then S.restore() end
    end
end
uevr.sdk.callbacks.on_pre_viewport_client_draw(function()
    local ok,err=pcall(apply)
    if not ok then S.enabled=false;S.restore();S.status=tostring(err) end
    if not S.stopped and (S.ticks==1 or S.ticks%120==0) then  end
end)
pcall(function()uevr.sdk.callbacks.on_script_reset(S.restore)end)

end

end)(AC8Runtime.host('layout','forward_locator.lua'),AC8Runtime.json,AC8Runtime.log)
end

-- Fixed HUD-container adjustments. No steady-state geometry inspection.
do
local R=AC8Runtime;local host=R.host('layout','fixed arrow and wingman status placement')
local S={version=61,epoch=-1,x=0,scale=1,nextControls=0,attempts=0};R.offscreenLocatorMargin=S
local W={version=61,gap=24};R.wingmanSelector=W;R.wingmanStatusLayout=W
local function invokeInventory(o,k,...)if AC8Runtime.inventory then return AC8Runtime.inventory.call(o,k,...)end;return o[k](o,...)end
local function call(o,k,...)if not o or(o~=uevr.api and not R.isLiveObject(o))then return end;local ok,v=pcall(invokeInventory,o,k,...);if ok then return v end end
local function full(o)return call(o,'get_full_name')end
local function xy(v)local ok,x,y=pcall(function()return v.X,v.Y end);if ok and type(x)=='number'and type(y)=='number'and x==x and y==y then return{X=x,Y=y}end end
local function same(a,b)return a and b and math.abs(a.X-b.X)<.01 and math.abs(a.Y-b.Y)<.01 end
local function translation(o)if not R.isLiveObject(o)then return end;local ok,v=pcall(function()return o:get_property('RenderTransform').Translation end);return ok and xy(v)end
local function put(o,v)call(o,'SetRenderTranslation',v);return same(translation(o),v)end
local function scale(o)local ok,v=pcall(function()return o:get_property('RenderTransform').Scale end);return ok and xy(v)end
local function pivot(o)return xy(call(o,'GetRenderTransformPivot')or call(o,'get_property','RenderTransformPivot'))end
local function putScale(o,v)call(o,'SetRenderScale',v);return same(scale(o),v)end

local function parent(o)
 local p=call(o,'GetParent');if p then return p end
 local outer=call(o,'get_outer');if(full(outer)or''):match('^WidgetTree ')then return call(outer,'get_outer')end
end
local slate=call(call(uevr.api,'find_uobject','Class /Script/UMG.SlateBlueprintLibrary'),'get_class_default_object')
local function geo(o)return call(o,'GetPaintSpaceGeometry')or call(o,'GetCachedGeometry')end
local function point(g,p)return xy(call(slate,'LocalToAbsolute',g,p))end
local function rect(o)
 local g=geo(o);local size=xy(call(slate,'GetLocalSize',g));if not size or size.X<=0 or size.Y<=0 then return end
 local a=point(g,{X=0,Y=0});local b=point(g,size);if not a or not b then return end
 return{math.min(a.X,b.X),math.min(a.Y,b.Y),math.max(a.X,b.X),math.max(a.Y,b.Y)}
end
local function restore(row)
 if row.widget and R.isLiveObject(row.widget)and full(row.widget)==row.name and same(translation(row.widget),row.last)then put(row.widget,row.original)end
 if row.widget and R.isLiveObject(row.widget)then
  if row.originalScale and same(scale(row.widget),row.lastScale)then putScale(row.widget,row.originalScale)end
  if row.originalPivot and same(pivot(row.widget),row.lastPivot)then call(row.widget,'SetRenderTransformPivot',row.originalPivot)end
 end
 row.widget=nil;row.name=nil;row.last=nil;row.applied=false
end
local function discard(row)row.widget=nil;row.name=nil;row.last=nil;row.original=nil;row.originalScale=nil;row.originalPivot=nil;row.lastScale=nil;row.lastPivot=nil;row.applied=false end
local function findWidgets()
 local c=R.context;local prefix=(c.rootName or''):match('^%S+ (.+)$');if not prefix then return end
 -- These non-variable Blueprint tree nodes are not UProperties on HudWidget.
 local cls=call(uevr.api,'find_uobject','Class /Script/UMG.CanvasPanel')
 for _,o in ipairs(call(cls,'get_objects_matching',false)or{})do
  local n=full(o)or''
  if n:find(' '..prefix..'.',1,true)then
   if not S.widget and n:match('%.LowestPriorityDynamicDrawCanvas$')then S.widget=o;S.name=n;S.canvas=n;S.original=translation(o);S.originalScale=scale(o);S.originalPivot=pivot(o)end
   if not W.widget and n:find('.WBP_HUD_Command_Wingman_000.',1,true)and n:match('%.Command_S$')then W.widget=o;W.name=n;W.original=translation(o)end
  end
 end
end
local function placeStatus()
 if W.applied or not W.widget or not W.original then return end
 local refs=R.hudLayoutRefs;local radar=rect(refs and refs.radar);if not radar then W.status='waiting for radar paint';return end
 local content=rect(W.widget)
 if not content then
  -- The zero-size outer canvas may have painted icon children extending outside.
  local cls=call(uevr.api,'find_uobject','Class /Script/UMG.Widget')
  for _,o in ipairs(call(cls,'get_objects_matching',false)or{})do
   local n=full(o)or''
   if n:find('.WBP_HUD_Command_Wingman_000.',1,true)and(n:match('^Image ')or n:match('^TextBlock ')or n:match('^LocalizeTextBlock '))then
    local p=o
    for _=1,16 do
     p=parent(p);local pn=full(p);if not pn then break end
     if pn==W.name then
      local b=rect(o);if b then
       if not content then content=b else content={math.min(content[1],b[1]),math.min(content[2],b[2]),math.max(content[3],b[3]),math.max(content[4],b[4])}end
      end;break
     end
    end
   end
  end
 end
 if not content then W.status='status icon not painted; no guessed position';return end
 local pg=geo(parent(W.widget));local a=point(pg,{X=0,Y=0});local b=point(pg,{X=1,Y=1})
 if not a or not b or b.X<=a.X or b.Y<=a.Y then W.status='status parent geometry unavailable';return end
 local current=translation(W.widget);if not current then return end
 local wanted={X=current.X+(radar[1]-content[1])/(b.X-a.X),Y=current.Y+(radar[2]-content[4])/(b.Y-a.Y)-W.gap}
 if put(W.widget,wanted)then W.last=wanted;W.applied=true;W.status='small green Command_S status above radar, left aligned'
 else W.status='status translation readback failed'end
 W.radarBounds=radar;W.contentBounds=content
end


host.sdk.callbacks.on_post_engine_tick(function()
 local c=R.context
 if not c.flight then discard(S);discard(W);S.epoch=-1;return end
 if not gameplay(AC8Runtime)then return end
 if c.epoch~=S.epoch then
  discard(S);discard(W);S.epoch=c.epoch;S.attempts=0;S.nextAttempt=0;S.nextControls=0;W.status='waiting for status icon'
  W.enabled=false -- Initial placement precedes painted radar geometry availability
 end

 if not S.controlsLoaded then
  S.controlsLoaded=true -- Reload preferences on script/game restart.
  local control={} -- TEMP baseline: saved offscreen margins remain on disk.
  local x=tonumber(control.offsetX)or 0;local factor=tonumber(control.scale)or 1
  if x~=x or x< -400 or x>0 then x=0 end
  if factor~=factor or factor<.8 or factor>1 then factor=1 end
  if control.enabled==false then x=0;factor=1 end
  if x~=S.x or factor~=S.scale then S.x=x;S.scale=factor;S.applied=false;S.attempts=0;S.nextAttempt=0 end
 end
 if S.applied and(W.applied or not W.enabled)then return end
 -- At most eight initialization attempts; steady state is a cached early exit.
 if S.attempts>=8 or R.tick<(S.nextAttempt or 0)then return end
 S.attempts=S.attempts+1;S.nextAttempt=R.tick+60
 if not S.widget or(W.enabled and not W.widget)then findWidgets()end
 if not S.applied and S.widget and S.original and S.originalScale and S.originalPivot then
  local wanted={X=S.original.X+S.x,Y=S.original.Y}
  local wantedScale={X=S.originalScale.X*S.scale,Y=S.originalScale.Y*S.scale}
  local wantedPivot=S.originalPivot;call(S.widget,'SetRenderTransformPivot',wantedPivot)
  if put(S.widget,wanted)and putScale(S.widget,wantedScale)and same(pivot(S.widget),wantedPivot)then
   S.last=wanted;S.lastScale=wantedScale;S.lastPivot=wantedPivot;S.applied=true;S.status='native off-screen canvas restored; baseline has no margin/scale nudge'
  else S.status='container setter readback failed'end
 end
 if W.enabled then placeStatus()end
 
end)
pcall(function()host.sdk.callbacks.on_script_reset(function()restore(S);restore(W)end)end)
end

-- V76: final accepted bomb calibration. No key capture or telemetry.
do
local R=AC8Runtime;local host=R.host('layout','weapon projection calibration')
local sent={}
host.sdk.callbacks.on_post_engine_tick(function()
 local w=simpleHUD.weapon;local flight=gameplay(R)
 if w.bombFov~=sent.fov or w.bombY~=sent.y or flight~=sent.bombFlight then
  uevr.api:dispatch_custom_event('AC8_BombProjectionControls',string.format('1|%.8f|0|%d|%.8f',w.bombFov,flight and 1 or 0,w.bombY))
  sent.fov,sent.y,sent.bombFlight=w.bombFov,w.bombY,flight
 end
 if w.saamScale~=sent.scale or flight~=sent.flight then
  uevr.api:dispatch_custom_event('AC8_WeaponCalibration',string.format('%.8f|0|%d',w.saamScale,flight and 1 or 0))
  sent.scale,sent.flight=w.saamScale,flight
 end
end)
host.sdk.callbacks.on_script_reset(function()
 uevr.api:dispatch_custom_event('AC8_BombProjectionControls','0|0|0|0')
 uevr.api:dispatch_custom_event('AC8_WeaponCalibration','1|0|0')
end)
end


-- V73: refresh the velocity-icon origin attachment every engine frame.
-- The shared line/circle FOV calibration below remains independent of the origin.
do
local R=AC8Runtime;local host=R.host('layout','bomb velocity origin anchor')
local S={version=73,epoch=-1,nextDiscover=0};R.bombLineAnchor=S
local function invokeInventory(o,k,...)if AC8Runtime.inventory then return AC8Runtime.inventory.call(o,k,...)end;return o[k](o,...)end
local function call(o,k,...)
 if o~=uevr.api and not R.isLiveObject(o)then return end
 local ok,v=pcall(invokeInventory,o,k,...);if ok then return v end
end
local function readField(o,k)return o[k]end
local function field(o,k)local ok,v=pcall(readField,o,k);if ok then return v end end
local function xy(v)
 local x,y=field(v,'X'),field(v,'Y')
 if type(x)=='number'and type(y)=='number'and x==x and y==y and math.abs(x)<1e7 and math.abs(y)<1e7 then return{X=x,Y=y}end
end
local function disable(reason)
 if S.packet then uevr.api:dispatch_custom_event('AC8_BombLineAnchor','0|0|0|0|0')end
 S.packet=nil;S.status=reason
end
local function discover()
 local root=R.context.root;if not R.isLiveObject(root)then return end
 local alignment=rawget(_G,'AC8_VelocityAlignment')
 local icon=alignment and alignment.icon
 local rootPath=(R.context.rootName or''):match('^%S+ (.+)$')
 if not R.isLiveObject(icon)then
  local persistent=call(root,'get_property','WBP_ChroniclePersistent')
  icon=call(persistent,'get_property','VelocityIcon')
 end
 local iconName=call(icon,'get_full_name')or''
 if not R.isLiveObject(icon)or not rootPath or not iconName:find(' '..rootPath..'.',1,true)or not iconName:find('.WBP_ChroniclePersistent.',1,true)then return end
 local bomb=call(root,'get_property','WBP_ExplosivePredicationWidget')
 if not R.isLiveObject(bomb)then
  local cls=call(uevr.api,'find_uobject','Class /Script/Live.LiveExplosivePredicationWidget')
  for i,o in ipairs(call(cls,'get_objects_matching',false)or{})do
   if i>16 then break end
   if rootPath and(call(o,'get_full_name')or''):find(' '..rootPath..'.',1,true)and call(o,'get_property','CachedLiveHUD')==R.context.hud then bomb=o;break end
  end
 end
 if call(bomb,'get_property','CachedLiveHUD')~=R.context.hud then return end
 local canvas=call(bomb,'GetParent')
 if not R.isLiveObject(bomb)or not(call(canvas,'get_full_name')or''):match('^LiveDynamicDrawCanvas ')then return end
 S.icon=icon;S.iconName=iconName;S.bomb=bomb;S.canvas=canvas
 S.slate=call(call(uevr.api,'find_uobject','Class /Script/UMG.SlateBlueprintLibrary'),'get_class_default_object')
end
local function update()
 local c=R.context
 if S.epoch~=c.epoch then disable('waiting for current HUD');S.epoch=c.epoch;S.icon=nil;S.bomb=nil;S.canvas=nil;S.nextDiscover=0 end
 if not gameplay(AC8Runtime)then disable('outside gameplay presentation');return end
 if not R.isLiveObject(S.icon)or not R.isLiveObject(S.bomb)or not R.isLiveObject(S.canvas)or not R.isLiveObject(S.slate)then
  disable('waiting for velocity icon and bomb canvas')
  if R.tick<S.nextDiscover then return end;S.nextDiscover=R.tick+60;discover()
 end
 if not R.isLiveObject(S.icon)or not R.isLiveObject(S.bomb)or not R.isLiveObject(S.canvas)then return end
 local ig=call(S.icon,'GetPaintSpaceGeometry')or call(S.icon,'GetCachedGeometry')
 local cg=call(S.canvas,'GetPaintSpaceGeometry')or call(S.canvas,'GetCachedGeometry')
 if not ig or not cg then disable('waiting for painted geometry');return end
 local is=xy(call(S.slate,'GetLocalSize',ig));local cs=xy(call(S.slate,'GetLocalSize',cg))
 if not is or not cs or is.X<=0 or is.Y<=0 or cs.X<1000 or cs.Y<500 then disable('waiting for painted geometry');return end
 local absolute=xy(call(S.slate,'LocalToAbsolute',ig,{X=is.X/2,Y=is.Y/2}))
 local target=absolute and xy(call(S.slate,'AbsoluteToLocal',cg,absolute))
 local widget=call(S.bomb,'get_address');local canvas=call(S.canvas,'get_address')
 if not target or type(widget)~='number'or type(canvas)~='number'then disable('invalid origin geometry');return end
 if target.X<0 or target.Y<0 or target.X>cs.X or target.Y>cs.Y then disable('velocity icon outside canvas');return end
 local packet=string.format('1|%.0f|%.0f|%.8f|%.8f',widget,canvas,target.X,target.Y)
 if packet~=S.packet or R.tick-(S.sentAt or 0)>=12 then
  uevr.api:dispatch_custom_event('AC8_BombLineAnchor',packet);S.packet=packet;S.sentAt=R.tick
 end
 S.status='top follows velocity icon every frame; lower endpoint and circle share calibrated projection'
 S.target=target;S.absoluteCenter=absolute;S.canvasSize=cs
end
host.sdk.callbacks.on_post_engine_tick(update)
host.sdk.callbacks.on_script_reset(function()disable('script reset');uevr.api:dispatch_custom_event('AC8_BombProjectionControls','0|0|0|0')end)
end

-- V81: Command_S stays on the stable HUD canvas. Cached radar geometry supplies
-- only Y; saved X remains independent of radar size and parent adjustments.
do
local R=AC8Runtime;local host=R.host('layout','cached wingman radar-top placement')
local S={version=81,epoch=-1,nextFind=0,nextRead=0,nextCheck=0};R.wingmanStatusLayout=S;R.wingmanSelector=S
local file='AC8_VR-WingmanStatusControls.json'
local function invokeInventory(o,k,...)if AC8Runtime.inventory then return AC8Runtime.inventory.call(o,k,...)end;return o[k](o,...)end
local function call(o,k,...)
 if not o or(o~=uevr.api and not R.isLiveObject(o))then return end
 local ok,v=pcall(invokeInventory,o,k,...);if ok then return v end
end
local function full(o)return call(o,'get_full_name')end
local function readField(o,k)return o[k]end
local function field(o,k)local ok,v=pcall(readField,o,k);if ok then return v end end
local function xy(v)local x,y=field(v,'X'),field(v,'Y');if type(x)=='number'and type(y)=='number'and x==x and y==y then return{X=x,Y=y}end end
local function same(a,b,epsilon)return a and b and math.abs(a.X-b.X)<(epsilon or .1)and math.abs(a.Y-b.Y)<(epsilon or .1)end
local zero={X=0,Y=0}
local slate=call(call(uevr.api,'find_uobject','Class /Script/UMG.SlateBlueprintLibrary'),'get_class_default_object')
local function normalize(c)
 c=type(c)=='table'and c or{}
 local function number(k,default,lo,hi)local v=tonumber(c[k]);return v and v==v and v>=lo and v<=hi and v or default end
 return{enabled=c.enabled~=false,x=number('x',0,-1000,5000),y=number('y',1460,-1000,3200),width=number('width',440,1,2000),height=number('height',96,1,1000),liveAdjust=c.liveAdjust~=false,followRadarTop=c.followRadarTop~=false,radarGap=number('radarGap',24,0,300)}
end
local function slotLayout(slot)
 local offsets=call(slot,'GetOffsets');local anchors=call(slot,'GetAnchors');local alignment=xy(call(slot,'GetAlignment'))
 local min,max=xy(field(anchors,'Minimum')),xy(field(anchors,'Maximum'))
 local margin={Left=field(offsets,'Left'),Top=field(offsets,'Top'),Right=field(offsets,'Right'),Bottom=field(offsets,'Bottom')}
 if not alignment or not min or not max then return end
 for _,v in pairs(margin)do if type(v)~='number'then return end end
 return{offsets=margin,anchors={Minimum=min,Maximum=max},alignment=alignment,auto=call(slot,'GetAutoSize'),z=call(slot,'GetZOrder')}
end
local function restore()
 if S.reparented and R.isLiveObject(S.widget)and R.isLiveObject(S.originalParent)then
  call(S.widget,'RemoveFromParent');local slot=call(S.originalParent,'AddChildToCanvas',S.widget)
  if slot and S.originalLayout then
   local c=S.originalLayout;call(slot,'SetAnchors',c.anchors);call(slot,'SetOffsets',c.offsets);call(slot,'SetAlignment',c.alignment)
   call(slot,'SetAutoSize',c.auto);call(slot,'SetZOrder',c.z)
  end
  if S.original then call(S.widget,'SetRenderTranslation',S.original)end
  if S.originalScale then call(S.widget,'SetRenderScale',S.originalScale)end
 end
 S.widget=nil;S.canvas=nil;S.slot=nil;S.reparented=false;S.last=nil;S.radarState=nil
 S.small=nil;S.large=nil;S.expanded=nil;S.searchWidget=nil
 S.scaleOwned=nil
end
local function find(forceGlobal)
 local prefix=(R.context.rootName or''):match('^%S+ (.+)$');if not prefix then return end
 local owned,names
 if not forceGlobal then owned,names=R.ownedWidgets(R.context.root)end
 local function locate(o,n)
  if not n or not n:find(' '..prefix..'.',1,true)then return end
  if n:match('^CanvasPanel ')then
   if n:match('%.AlwaysVisibleCanvas$')then S.canvas=o end
   if n:find('.WBP_HUD_Command_Wingman_000.',1,true)and n:match('%.Command_S$')then S.widget=o;S.name=n end
   if n:find('.MiniMapLayer.',1,true)then
    if n:match('%.MiniMapCornerCanvas$')then S.large=o end
    if n:match('%.MiniMapEnlargedCorners$')then S.expanded=o end
   end
  elseif n:match('^Border ')and n:find('.MiniMapLayer.',1,true)and n:match('%.RadarBoundingArea$')then
   S.small=o;local tree=call(o,'get_outer');if(full(tree)or''):match('^WidgetTree ')then S.searchWidget=call(tree,'get_outer')end
  end
 end
 if owned then
  for _,o in ipairs(owned)do if R.isLiveObject(o)then locate(o,names[o])end end
  local missingRadar=S.cfg.followRadarTop and not(S.small and S.large and S.expanded and S.searchWidget)
  if not S.canvas or not S.widget or missingRadar then return find(true)end
 else
  for _,class in ipairs({'CanvasPanel','Border'})do
   local cls=call(uevr.api,'find_uobject','Class /Script/UMG.'..class)
   for _,o in ipairs(call(cls,'get_objects_matching',false)or{})do locate(o,full(o))end
  end
 end
 if not S.canvas or not S.widget then S.status='waiting for Command_S and stable canvas';return end
 S.originalParent=call(S.widget,'GetParent');local oldSlot=call(S.widget,'get_property','Slot')
 S.originalScale=xy(field(call(S.widget,'get_property','RenderTransform'),'Scale'))
 S.originalLayout=slotLayout(oldSlot);S.original=xy(field(call(S.widget,'get_property','RenderTransform'),'Translation'))
 if not R.isLiveObject(S.originalParent)or not S.originalLayout or not S.original then S.widget=nil;S.status='original slot unavailable';return end
 S.reparented=true;call(S.widget,'RemoveFromParent')
 local slot=call(S.canvas,'AddChildToCanvas',S.widget)
 if not R.isLiveObject(slot)then restore();S.status='stable canvas attachment failed; original restored';return end
 S.slot=slot;S.reparented=true
 call(slot,'SetAnchors',{Minimum=zero,Maximum=zero});call(slot,'SetAlignment',zero);call(slot,'SetAutoSize',false)
 call(slot,'SetZOrder',S.originalLayout.z or 0);call(S.widget,'SetRenderTranslation',zero)
 S.status='Command_S attached to stable HUD canvas'
end
local function radarTop(c)
 if not c.followRadarTop then return end
 local visibility=call(S.expanded,'GetVisibility');local searchVisibility=call(S.searchWidget,'GetVisibility')
 local expanded=visibility==0 or visibility==3 or visibility==4 or searchVisibility==1 or searchVisibility==2
 local widget=expanded and S.large or S.small
 local g=call(widget,'GetPaintSpaceGeometry')or call(widget,'GetCachedGeometry')
 local cg=call(S.canvas,'GetPaintSpaceGeometry')or call(S.canvas,'GetCachedGeometry')
 local size=xy(call(slate,'GetLocalSize',g));local canvasSize=xy(call(slate,'GetLocalSize',cg))
 if not size or not canvasSize or size.X<1 or size.Y<1 or canvasSize.X<1 or canvasSize.Y<1 then return end
 local absolute=xy(call(slate,'LocalToAbsolute',g,zero));local top=absolute and xy(call(slate,'AbsoluteToLocal',cg,absolute))
 if not top or top.Y<0 or top.Y>canvasSize.Y or top.X< -1000 or top.X>canvasSize.X then return end
 S.radarState={mode=expanded and'expanded' or'small',expandedVisibility=visibility,searchVisibility=searchVisibility,name=full(widget),top=top,localSize=size}
 top=simpleHUD.navigationUngroupPoint(S.canvas,top,widget)
 if not top then return end
 return top.Y-c.height-c.radarGap
end
local function apply(c)
 
 local move=simpleHUD.parentOffset(S.canvas,{'wingman'})
 if not move then S.status='waiting for wingman group geometry';return end
 local radarY=radarTop(c);local y=radarY or c.y
 local wanted={X=c.x+move.X,Y=y+move.Y};local size={X=c.width,Y=c.height}
 local scale=simpleHUD.navigationScale()*simpleHUD.scale('wingman')
 if simpleHUD.navigationActive()then
  local mapped=simpleHUD.navigationPoint(S.canvas,wanted)
  if not mapped then S.status='waiting for navigation group geometry';return end
  wanted=mapped
 end
 local baseScale=S.originalScale
 local transform=call(S.widget,'get_property','RenderTransform')
 if baseScale and(scale~=1 or S.scaleOwned)then
  local current=xy(field(transform,'Scale'))
  local scaled={X=baseScale.X*scale,Y=baseScale.Y*scale}
  if not same(current,scaled,.001)then
   call(S.widget,'SetRenderScale',scaled)
   transform=call(S.widget,'get_property','RenderTransform')
   current=xy(field(transform,'Scale'))
  end
  S.scaleOwned=scale~=1 or not same(current,scaled,.001)
 end
 local position=xy(call(S.slot,'GetPosition'));local actualSize=xy(call(S.slot,'GetSize'))
 local translation=xy(field(transform,'Translation'))
 if not same(position,wanted)then call(S.slot,'SetPosition',wanted);end
 if not same(actualSize,size)then call(S.slot,'SetSize',size);end
 if translation and not same(translation,zero)then call(S.widget,'SetRenderTranslation',zero);end
 S.last=wanted;S.actual=position;S.status=S.radarState and'fixed X; Y follows displayed radar top' or'fixed fallback position; radar geometry unavailable'
end
simpleHUD.applyWingman=function()
 if not(simpleHUD.navigationActive()or S.navigationOwned)then return end
 if S.reparented and S.cfg and S.cfg.enabled and R.context.flight and not S.cinematicSuspended and R.isLiveObject(S.slot)then apply(S.cfg)end
end
host.sdk.callbacks.on_post_engine_tick(function()
 local ctx=R.context
 if S.epoch~=ctx.epoch then restore();S.epoch=ctx.epoch;S.nextFind=0;S.nextRead=0;S.nextCheck=0 end
 local ci=rawget(_G,'AC8_CinematicIntent')
 local cinematic=ci and(ci.maskActive or ci.detectedMode=='cinematic')
 if cinematic then
  if S.reparented then S.cinematicWidget=S.widget;restore()end
  S.cinematicSuspended=true;S.status='cinematic: original HUD parent owns visibility';return
 end
 if S.cinematicSuspended then
  S.cinematicSuspended=false;S.cinematicWidget=nil;S.nextFind=0;S.nextCheck=0
 end
 if not ctx.flight then return end
 if not S.cfg then S.cfg=normalize(R.read(file))end -- Reload preferences on script/game restart.
 if not S.cfg.enabled then if S.reparented then restore()end;return end
 if R.tick<S.nextCheck then return end;S.nextCheck=R.tick+3
 if not R.isLiveObject(S.widget)or not R.isLiveObject(S.slot)then
  if R.tick<S.nextFind then return end;S.nextFind=R.tick+60;find()
 end
 if S.reparented and R.isLiveObject(S.slot)then apply(S.cfg)end
end)
host.sdk.callbacks.on_script_reset(restore)
end

-- TEMPORARY LIVE ARROW CALIBRATION START: remove after tuning.
do
local R=AC8Runtime;local host=R.host('layout','temporary live arrow calibration')
local realJSON=json;local file='AC8_VR-ArrowLiveControls.json'
local function number(v,limit)
 v=tonumber(v);if not v or v~=v or math.abs(v)>limit then return 0 end
 return math.floor(v+.5)
end
local function normalized(v)
 v=type(v)=='table'and v or{}
 return{linked=v.linked~=false,commonX=number(v.commonX,2000),commonY=number(v.commonY,2000),
  noseX=number(v.noseX,1000),noseY=number(v.noseY,1000),targetX=number(v.targetX,1000),targetY=number(v.targetY,1000)}
end
local S={temporary=true,values=normalized({}) --[[TEMP baseline: retain saved arrow trims without applying them.]],nativeStatus='waiting',stopped=false}
R.arrowLiveControls=S
local function dispatch(name,data)uevr.api:dispatch_custom_event(name,data)end
local function totals()
 local v=S.values;return v.commonX+v.noseX,v.commonY+v.noseY,v.commonX+v.targetX,v.commonY+v.targetY
end
local function publish()
 local nx,ny,tx,ty=totals();local packet=string.format('%.1f|%.1f|%.1f|%.1f',nx,ny,tx,ty)
 if packet~=S.packet then dispatch('AC8_ArrowLiveControls',packet);S.packet=packet end
end

publish() -- One startup packet; no engine-tick/file polling added.
host.sdk.callbacks.on_lua_event(function(event,data)
 if S.stopped then return end
 if event=='AC8_ArrowLiveStatus'then
  if data=='ready'or data=='waiting'or data=='unavailable'then S.nativeStatus=data end
 end
end)
host.sdk.callbacks.on_script_reset(function()S.stopped=true;dispatch('AC8_ArrowLiveControls','off')end)
end
-- TEMPORARY LIVE ARROW CALIBRATION END.




end


if registry.start then registry.start()end


