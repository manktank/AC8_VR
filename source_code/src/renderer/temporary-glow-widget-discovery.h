#pragma once
// Included inside widget_policy. Cooperative engine-thread work: one node or
// one slot per step, a 1ms target and 512-step ceiling per callback. A single
// SDK call cannot be preempted; measured overruns are reported, never concealed.
struct Discovery {
    using Array=selective_glow_inventory::Array;
    struct Entry {Leaf identity;std::array<Leaf,2> slate;};
    struct Check {Leaf identity,tree_identity,slot_identity;Object tree=nullptr,root=nullptr,slot=nullptr;const void* slots=nullptr;Array list{};unsigned depth=0;};
    struct Task {bool slots=false;Object object=nullptr;int parent=-1;unsigned depth=0;int32_t cursor=0;Object slot=nullptr;bool notification_only=false;};
    struct Root {const wchar_t* key=nullptr;Object value=nullptr;};
    Walk walk;std::vector<Check> checks;std::vector<Task> tasks;std::vector<Root> roots;
    std::vector<unsigned char> required;
    std::unordered_map<Object,Entry> previous,next;
    Leaf hud,owner,widget_class;
    bool priority=false;
    //0=legacy combined fixture,1=notification,2=tutorial prompt,3=dialog,4=input.
    unsigned component=0;
    explicit Discovery(bool urgent=false,unsigned part=0):priority(urgent),component(part){}
    std::array<Object,128> dialogs{};
    int dialog_count=0;
    size_t cursor=0,root_cursor=0;
    unsigned stage=0,root_kind=0;
    uint64_t started_at=0;
    bool running=false;
    std::shared_ptr<Roster> roster;
    static bool same(const Array& a,const Array& b){return a.data==b.data&&a.count==b.count&&a.capacity==b.capacity;}
    static bool array_valid(const Array& a){return a.count>=0&&a.count<=4096&&a.capacity>=a.count&&a.capacity<=65536&&(!a.count||a.data);}
    void fail(const char* reason){walk.failure=reason;}
    bool identity(Object object,Leaf& leaf){
        const auto old=previous.find(object);
        if(old!=previous.end()&&identity_valid(old->second.identity)){leaf=old->second.identity;return true;}
        return make_identity(object,leaf);
    }
    void begin(Session& session){
        if(!session.discovery_started_at)session.discovery_started_at=GetTickCount64();
        walk.nodes.clear();walk.seen.clear();walk.failure=nullptr;checks.clear();tasks.clear();roots.clear();required.clear();next.clear();cursor=root_cursor=0;stage=0;root_kind=component==4?1u:0u;dialog_count=0;running=true;started_at=GetTickCount64();
        roster=std::make_shared<Roster>();roster->revision=session.refreshes.load()+1;
        if(!selective_glow_inventory::available(api)||!api->sdk->uobject->get_class||!api->sdk->uobject_hook->get_objects_by_class){fail("discovery_api_unavailable");return;}
        if(!make_identity(session.hud,hud)||!make_identity(session.owner,owner)){fail("discovery_owner_identity");return;}
        // One base-class lookup per session (or class replacement), never per
        // widget or per weak field. Serial/header proof prevents address reuse.
        if(!identity_valid(widget_class)){
            ++session.discovery_class_queries;
            const auto cls=api->sdk->uobject_array->find_uobject(L"Class /Script/UMG.Widget");
            if(!make_identity(cls,widget_class)){fail("widget_base_class_unavailable");return;}
        }
    }
    bool attach_node(const Task& task){
        const auto object=task.object;
        if(!object||task.depth>=64||walk.nodes.size()>=8192){fail("tree_lifetime_depth_or_capacity");return false;}
        if(walk.seen.count(object))return true;
        Check check;check.depth=task.depth;check.slot=task.slot;
        if(!identity(object,check.identity)){fail("tree_object_identity");return false;}
        const auto cls=walk.class_path(object);unsigned role=class_role(cls);
        const unsigned search_mask=component==2?(tutorial_text|button_hint):notification;
        const bool notification_search=task.notification_only&&!(role&search_mask);
        if(notification_search)role=0;
        if(cls=="/Script/UMG.Image"){
            wchar_t name[256]{};api->sdk->fname->to_string(api->sdk->uobject->get_fname(object),name,256);
            const auto tree=api->sdk->uobject->get_outer(object);
            const auto outer=selective_glow_inventory::live(api,tree)?api->sdk->uobject->get_outer(tree):nullptr;
            if(selective_glow_inventory::live(api,outer))role|=named_role(cls,name,walk.class_path(outer));
        }
        const auto index=static_cast<int>(walk.nodes.size());
        if(!walk.object(object,L"WidgetTree",check.tree)){fail("widget_tree_read");return false;}
        if(check.tree&&(!make_identity(check.tree,check.tree_identity)||api->sdk->uobject->get_outer(check.tree)!=object||
            !walk.object(check.tree,L"RootWidget",check.root)||(check.root&&api->sdk->uobject->get_outer(check.root)!=check.tree))){fail("widget_tree_ownership");return false;}
        if(check.slot&&!make_identity(check.slot,check.slot_identity)){fail("panel_slot_identity");return false;}
        // Native target/world canvas pools contain transient generated labels,
        // not the authored tutorial/message trees selected by this policy.
        check.slots=cls=="/Script/Live.LiveDynamicDrawCanvas"?nullptr:walk.field(object,L"Slots");if(walk.failure)return false;
        if(check.slots&&(!local_read(check.slots,check.list)||!array_valid(check.list))){fail("panel_slots_bounds");return false;}
        walk.seen.emplace(object,walk.nodes.size());walk.nodes.push_back({object,task.parent,role,0,dialog_class(cls)});checks.push_back(std::move(check));
        const auto& stored=checks.back();
        // Bound HUD roots own many unrelated UserWidgets. Search only plain
        // panel ancestry until this component's exact selector is reached, then
        // validate its complete subtree. Notification and tutorial prompts are
        // independent jobs, so neither waits for the other's cold descendants.
        const bool unrelated_tree=notification_search&&task.depth>0&&stored.tree;
        if(!unrelated_tree){
            if(stored.list.count)tasks.push_back({true,object,index,task.depth,0,nullptr,notification_search});
            if(stored.root)tasks.push_back({false,stored.root,index,task.depth+1,0,nullptr,notification_search});
        }
        return true;
    }
    bool slot_step(Task task){
        const auto& parent=checks[static_cast<size_t>(task.parent)];Array observed{};
        if(!identity_valid(parent.identity)||!local_read(parent.slots,observed)||!array_valid(observed)){fail("panel_changed_during_discovery");return false;}
        // Never follow stale storage or chase a changing branch forever.
        // Found selected paths are independently revalidated before publication;
        // newly added children enter on the next bounded refresh.
        if(!same(observed,parent.list))return true;
        Object slot=nullptr,panel=nullptr,child=nullptr,back=nullptr;
        if(!local_read(observed.data+task.cursor,slot)||!walk.object(slot,L"Parent",panel)||panel!=task.object||
            !walk.object(slot,L"Content",child)||!child||!walk.object(child,L"Slot",back)||back!=slot){fail("panel_slot_ownership");return false;}
        if(++task.cursor<observed.count)tasks.push_back(task);
        tasks.push_back({false,child,task.parent,task.depth+1,0,slot,task.notification_only});return true;
    }
    bool validate(size_t index){
        const auto& check=checks[index];const auto& node=walk.nodes[index];Object tree=nullptr,root=nullptr;
        const auto reject=[&](const char* detail){roster->failed_object=node.object;roster->failure_detail=detail;return false;};
        if(!identity_valid(check.identity))return reject("object_header_or_registry_serial_changed");
        if(!walk.object(node.object,L"WidgetTree",tree)||tree!=check.tree)return reject("widget_tree_binding_changed");
        if(tree&&(!identity_valid(check.tree_identity)||api->sdk->uobject->get_outer(tree)!=node.object||!walk.object(tree,L"RootWidget",root)||root!=check.root||
            (root&&api->sdk->uobject->get_outer(root)!=tree)))return reject("widget_tree_ownership_changed");
        if(check.slots){Array observed{};if(!local_read(check.slots,observed)||!array_valid(observed))return reject("panel_slots_invalid");}
        if(check.slot){Object panel=nullptr,child=nullptr,back=nullptr;
            if(!identity_valid(check.slot_identity)||node.parent<0||!walk.object(check.slot,L"Parent",panel)||panel!=walk.nodes[static_cast<size_t>(node.parent)].object||
                !walk.object(check.slot,L"Content",child)||child!=node.object||!walk.object(node.object,L"Slot",back)||back!=check.slot)return reject("panel_slot_ownership_changed");
            const auto& parent=checks[static_cast<size_t>(node.parent)];Array list{};
            if(!parent.slots||!local_read(parent.slots,list)||!array_valid(list))return reject("parent_slots_invalid");
            std::vector<Object> slots(static_cast<size_t>(list.count));
            if(list.count&&!local_copy(slots.data(),list.data,slots.size()*sizeof(Object)))return reject("parent_slots_unreadable");
            if(std::find(slots.begin(),slots.end(),check.slot)==slots.end())return reject("slot_detached_from_parent");
        }return true;
    }
    void leaf_step(Session& session,size_t index){
        const auto& node=walk.nodes[index];Entry entry;entry.identity=checks[index].identity;
        if(!node.role){next.emplace(node.object,std::move(entry));return;}
        for(unsigned i=0;i<4;++i)if(node.own_role&(4u<<i))++roster->roots[i];
        const auto old=previous.find(node.object);bool found=false,classified=false;std::array<SlateProbe,2> probes;
        // Cached child batches carry their own Slate owner, not necessarily
        // the selected root/retainer. Prove every selected child before publish;
        // root-only entry proofs covered fresh paint but let cached children
        // glow again. Normal menus and orphan input previews are no longer
        // global exclusion forests, reducing work without weakening identity.
        const Entry* peer=nullptr;
        const auto ordinary=session.discovery.get();
        if(priority&&ordinary&&ordinary!=this){const auto cached=ordinary->previous.find(node.object);if(cached!=ordinary->previous.end())peer=&cached->second;}
        for(unsigned i=0;i<2;++i){Leaf leaf;const uintptr_t field=i?0xf8:0x108;
            if(old!=previous.end()&&valid(old->second.slate[i],reinterpret_cast<void*>(old->second.slate[i].slate))){leaf=old->second.slate[i];++session.discovery_reused;}
            else if(peer&&valid(peer->slate[i],reinterpret_cast<void*>(peer->slate[i].slate))){leaf=peer->slate[i];++session.discovery_reused;}
            else{
                leaf=entry.identity;const bool attached=attach_slate(leaf,true,field,&probes[i],&session.slate_types);
                if(probes[i].unreadable)++roster->unreadable_candidates;if(!attached)continue;
                if(!classified){if(!identity_valid(widget_class)||!api->sdk->uobject->is_a(node.object,reinterpret_cast<UEVR_UClassHandle>(widget_class.object))){fail("selected_widget_class_changed");return;}classified=true;}
                // Selection depends on the verified class/tree/serial, not a
                // full diagnostic outer path. Do not rebuild ~1000 such paths.
                leaf.name=walk.class_path(node.object)+" [object="+std::to_string(reinterpret_cast<uintptr_t>(node.object))+", serial="+std::to_string(leaf.serial)+"]";++session.discovery_new;
            }
            leaf.roles=node.role;entry.slate[i]=leaf;add_leaf(*roster,std::move(leaf));found=true;
        }
        if(found){for(unsigned i=0;i<4;++i)if(node.role&(4u<<i))++roster->widgets[i];}
        else{
            ++roster->missing_slate;for(unsigned i=0;i<4;++i)if(node.role&(4u<<i))++roster->missing_by_role[i];
            if(roster->missing_examples.size()<16){const auto klass=walk.class_path(node.object);
                const bool repeated=std::any_of(roster->missing_examples.begin(),roster->missing_examples.end(),[&](const auto& sample){return sample.roles==node.role&&sample.klass==klass;});
                if(!repeated)roster->missing_examples.push_back({node.object,node.role,klass,probes});}
        }
        next.emplace(node.object,std::move(entry));
        if(roster->leaves.size()>2048)fail("selected_slate_capacity");
    }
    void finish(Session& session){
        roster->visited=static_cast<unsigned>(walk.nodes.size());roster->completed_at=GetTickCount64();
        if(walk.failure){roster->failure=walk.failure;roster->leaves.clear();roster->roots={};roster->widgets={};if(!priority)++session.discovery_failures;else ++session.priority_failures;
            std::atomic_store(&session.last_failed_roster,std::shared_ptr<const Roster>(roster));}
        else if(!priority)++session.discovery_successes;
        // Reusing identity/Slate proofs is independent of policy publication.
        // A changed tree cannot force another cold pass over stable objects.
        if(!next.empty())previous.swap(next);
        const auto now=GetTickCount64();
        if(priority&&component){const auto index=component-1;session.priority_parts[index]=roster;
            ++session.urgent_cycles[index];session.urgent_last_ms[index]=now-started_at;session.urgent_members[index]=roster->leaves.size();
            if(!session.urgent_first_ms[index].load()&&!roster->leaves.empty())session.urgent_first_ms[index]=std::max<uint64_t>(1,now-session.discovery_started_at);
            session.next_urgent_refresh[index]=now+(component<=2?50:100);
            ++session.priority_refreshes;session.priority_last_cycle_ms=now-started_at;
            if(!session.priority_has_published.load()&&!roster->leaves.empty()){session.priority_first_publish_ms=now-session.discovery_started_at;session.priority_has_published=true;}
        }else if(priority){session.priority_roster=roster;++session.priority_refreshes;session.priority_last_cycle_ms=now-started_at;
            if(!session.priority_has_published.load()&&!roster->leaves.empty()){session.priority_first_publish_ms=now-session.discovery_started_at;session.priority_has_published=true;}
            session.next_priority_refresh=now+100;
        }else{session.ordinary_roster=roster;++session.refreshes;session.discovery_last_cycle_ms=now-started_at;session.next_refresh=now+500;}
        publish_rosters(session);running=false;
    }
    void step(Session& session){
        if(walk.failure){finish(session);return;}
        static const wchar_t* keys[]{L"HudWidget",L"SubtitleWidget",L"HUDMessageWidget",L"NonGlowWidget",L"GlowWidgetPtr",L"NonGlowFrontWidgetPtr",L"NonGlowBackWidgetPtr"};
        if(stage==0){
            if(priority){
                if(component>=3){stage=1;return;}
                const wchar_t* scoped[]{L"SubtitleWidget",L"HUDMessageWidget"};
                const unsigned count=component==2?2u:1u;
                if(root_cursor<count){Object object=nullptr;const auto key=scoped[root_cursor++];
                    if(!walk.object(session.hud,key,object)){fail("priority_hud_root_read");return;}
                    roots.push_back({key,object});if(object)tasks.push_back({false,object,-1,0,0,nullptr,true});return;}
                stage=component?3:1;return;
            }
            if(root_cursor<std::size(keys)){Object object=nullptr;const auto key=keys[root_cursor++];
                if(!walk.object(session.hud,key,object)){fail("hud_root_read");return;}roots.push_back({key,object});if(object)tasks.push_back({false,object,-1,0,0,nullptr});return;}
            stage=1;return;
        }
        if(stage==1){
            // UObjectHook indexes every native superclass, including cooked
            // descendants. These native bases are verified by the assets'
            // SuperIndex imports. A cooked class need not be loaded yet: asking
            // find_uobject for it every100ms caused unpreemptible global scans
            // and stalled both discovery queues. Resolve native bases once and
            // query their existing index as instances appear. Exact policy
            // class names below still decide which paint subtree is selected.
            static const wchar_t* types[]{L"Class /Script/Live.LiveNUICommonDialogWidget",
                L"Class /Script/Live.LiveNUIKeyGuideFooterWidget",
                L"Class /Script/Live.LiveMenuInputGuideWidget"};
            const auto type=types[root_kind];auto& type_identity=session.policy_classes[root_kind];
            const auto now=GetTickCount64();
            if(!identity_valid(type_identity)&&now>=session.policy_class_retry[root_kind]){++session.discovery_class_queries;const auto cls=api->sdk->uobject_array->find_uobject(type);session.policy_class_retry[root_kind]=now+1000;if(cls&&!make_identity(cls,type_identity)){fail("dialog_class_identity");return;}}
            // The SDK's by-class-name helper performs another global lookup.
            // Reuse the guarded class handle with its indexed object query.
            dialog_count=identity_valid(type_identity)?api->sdk->uobject_hook->get_objects_by_class(reinterpret_cast<UEVR_UClassHandle>(type_identity.object),dialogs.data(),static_cast<unsigned>(dialogs.size()),false):0;
            if(dialog_count<0||dialog_count>static_cast<int>(dialogs.size())){fail("dialog_lookup_bounds");return;}stage=2;cursor=0;return;
        }
        if(stage==2){if(cursor<static_cast<size_t>(dialog_count)){const auto object=dialogs[cursor++];if(selective_glow_inventory::live(api,object)&&owned(object,session.owner)){
                // CP_002 is found through actual reciprocal panel/tree links.
                // Enqueuing it as an independent root first lost its dialog
                // ancestry (UObject Outer is not a UMG parent in the real game).
                tasks.push_back({false,object,-1,0,0,nullptr});}return;}++root_kind;if(root_kind<session.policy_classes.size()&&component!=3)stage=1;else stage=3;return;}
        if(stage==3){if(!tasks.empty()){const auto task=tasks.back();tasks.pop_back();if(task.slots)slot_step(task);else attach_node(task);return;}stage=4;cursor=0;return;}
        // CP_002 promotes only its actual owning common dialog. Parents always
        // precede children, so inheritance is a separate, bounded linear pass.
        if(stage==4){if(cursor<walk.nodes.size()){const auto& node=walk.nodes[cursor++];if(node.own_role&tutorial_window){int at=node.parent;while(at>=0){auto& parent=walk.nodes[static_cast<size_t>(at)];if(parent.dialog){parent.own_role|=tutorial_window;break;}at=parent.parent;}}return;}stage=5;cursor=0;return;}
        if(stage==5){if(cursor<walk.nodes.size()){auto& node=walk.nodes[cursor++];node.role=node.own_role|(node.parent>=0?walk.nodes[static_cast<size_t>(node.parent)].role:0);return;}required.resize(walk.nodes.size());for(size_t i=0;i<walk.nodes.size();++i)if(walk.nodes[i].role){int at=static_cast<int>(i);while(at>=0&&!required[static_cast<size_t>(at)]){required[static_cast<size_t>(at)]=1;at=walk.nodes[static_cast<size_t>(at)].parent;}}stage=6;cursor=0;return;}
        if(stage==6){if(cursor<walk.nodes.size()){leaf_step(session,cursor++);return;}stage=7;cursor=0;return;}
        if(stage==7){if(cursor<checks.size()){const auto index=cursor++;if(required[index]&&!validate(index))fail("tree_changed_during_discovery");return;}stage=8;cursor=0;return;}
        if(cursor<roots.size()){const auto& root=roots[cursor++];Object object=nullptr;if(!walk.object(session.hud,root.key,object)||object!=root.value)fail("hud_root_changed_during_discovery");return;}
        finish(session);
    }
    template<class Clock>unsigned run_slice(Session& session,uint64_t begin,uint64_t budget,Clock clock,unsigned limit=512){
        if(running&&(!identity_valid(hud)||!identity_valid(owner)))fail("discovery_owner_changed");
        unsigned steps=0;while(running&&steps<limit&&clock()-begin<budget){step(session);++steps;}return steps;
    }
    void slice(Session& session,uint64_t begin,uint64_t budget){run_slice(session,begin,budget,[]{return selective_glow_performance::clock();});}
};
template<class Clock>unsigned run_discovery_queues(Session& session,Discovery& ordinary,Discovery& urgent,uint64_t begin,uint64_t budget,Clock clock,bool ordinary_first){
    unsigned steps=0;
    const auto run=[&](Discovery& queue){if(queue.running&&steps<512)steps+=queue.run_slice(session,begin,budget,clock,512-steps);};
    // The same1ms/512step budget is shared. Alternating first turn prevents a
    // busy modal forest from starving ordinary HUD discovery (as the captures
    // showed), while a new priority card waits at most one extra callback.
    if(ordinary_first){run(ordinary);run(urgent);}else{run(urgent);run(ordinary);}return steps;
}

// No callback is added. One budget is shared by all independent complete
// forests. Tiny cards publish as soon as their own ownership/Slate proof is
// complete, never as a partial dialog/input forest or an unproven cached owner.
template<class Clock>unsigned run_scoped_queues(Session& session,Discovery& ordinary,const std::array<Discovery*,4>& urgent,uint64_t begin,uint64_t budget,Clock clock){
    static constexpr unsigned order[]{0,1,2,0,2,1,3,4};
    unsigned steps=0,idle=0;
    while(steps<512&&clock()-begin<budget){
        const auto index=order[session.discovery_turn++%std::size(order)];
        auto* queue=index==4?&ordinary:urgent[index];
        if(!queue||!queue->running){if(++idle==std::size(order))break;continue;}
        idle=0;steps+=queue->run_slice(session,begin,budget,clock,1);
    }return steps;
}
