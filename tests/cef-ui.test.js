// No dependencies: exercise the actual CEF scripts with a small DOM/event stub.
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
function boot(resource) {
    const elements = {}, events = {}, emitted = [], timers = [];
    function element() {
        return {style:{setProperty(){}}, children:[], hidden:true, attrs:{}, textContent:'',
            dataset:{},value:'',listeners:{},
            addEventListener(k,fn){this.listeners[k]=fn;},focus(){},select(){},
            querySelector(){return null;},querySelectorAll(){return [];},
            classList:{add(){},remove(){},toggle(){}},
            setAttribute(k,v){this.attrs[k]=v;}, appendChild(el){this.children.push(el);},
            removeChild(el){this.children.splice(this.children.indexOf(el),1);}, remove(){},
            set textContent(v){this.text=v; this.children=[];}, get textContent(){return this.text;}};
    }
    const domEvents = {};
    const document = {getElementById(id){return elements[id] ||= element();},
        querySelector(id){return elements[id] ||= element();}, createElement:element,
        documentElement:element(), addEventListener(k,fn){domEvents[k]=fn;}};
    let frame;
    const context = {document, window:{cef:{on(k,fn){events[k]=fn;},emit(...args){emitted.push(args);}}},
        setTimeout(fn,ms){timers.push({fn,ms});}, clearTimeout(){}, requestAnimationFrame(fn){frame=fn;}, console};
    vm.runInNewContext(fs.readFileSync(`server/scriptfiles/cef/${resource}/app.js`,'utf8'),context);
    return {elements,events,emitted,timers,domEvents,frame:()=>frame()};
}
test('speedometer: zero and max needle agree with the 270 degree scale',()=>{
    const ui=boot('railway_speedo');
    assert.equal(ui.elements.needle.attrs.transform,'rotate(225 110 110)');
    assert.equal(ui.elements.arc.style.opacity,'0');
    assert.ok(ui.elements['.track'].style.strokeDasharray);
    ui.events['speedo:show']('Infernus',411,true);
    ui.events['speedo:update'](240,6,6000,11);
    for(let i=0;i<120;i++) ui.frame();
    assert.equal(ui.elements.needle.attrs.transform,'rotate(495 110 110)');
    assert.equal(ui.elements.speed.textContent,'240');
    assert.equal(ui.elements['gear-value'].textContent,'6');
    assert.equal(ui.emitted[0][0],'speedo:ready');
});
test('vehicle menu exposes actions, passenger ID and safe text',()=>{
    const ui=boot('railway_control');
    ui.events['control:vehicle']('Sultan',15,false);
    assert.equal(ui.elements.panel.hidden,false);
    assert.equal(ui.elements.content.children[0].children[0].textContent,'Закрыть двери');
    ui.elements.content.children[1].onclick();
    assert.deepEqual(Array.from(ui.emitted.at(-1)),['control:action',1,-1]);
    ui.events['control:passenger'](23,'<img onerror=bad()>');
    ui.elements.content.children.at(-1).onclick();
    assert.deepEqual(Array.from(ui.emitted.at(-1)),['control:action',4,23]);
    ui.events['control:hide'](); assert.equal(ui.elements.panel.hidden,true);
});
test('bindings reject duplicates and toasts drain for three seconds',()=>{
    const ui=boot('railway_control');
    ui.events['control:settings'](76,69,71);
    ui.elements.content.children[0].onclick();
    function press(keyCode){ui.domEvents.keydown({keyCode,key:String.fromCharCode(keyCode),preventDefault(){},stopPropagation(){}});}
    press(69);
    assert.notEqual(ui.emitted.at(-1)[0],'control:bind');
    assert.equal(ui.timers[0].ms,3000);
    press(75);
    assert.deepEqual(Array.from(ui.emitted.at(-1)),['control:bind',75,69,71]);
});
test('Pawn key filter allows Num/Caps Lock, blocks only Shift/Ctrl/Alt',()=>{
    const source=fs.readFileSync('server/pawno/include/railway/interaction.inc','latin1');
    const handler=source.slice(source.indexOf('public OnCefPressKey'));
    const match=handler.match(/modifiers & (\d+)/);
    assert.ok(match,'key handler must mask modifiers instead of rejecting every flag');
    const mask=Number(match[1]);
    assert.equal(mask & 1,0,'Caps Lock must not block E');
    assert.equal(mask & 256,0,'Num Lock must not block E');
    assert.equal(mask & (1|256),0);
    for (const flag of [2,4,8]) assert.ok(mask & flag);
    assert.ok(handler.includes('CEF_IsChatInputOpen(playerid)'));
});
test('Pawn fallback waits for page handshake, download progress cannot extend login indefinitely',()=>{
    const auth=fs.readFileSync('server/pawno/include/railway/accounts.inc','latin1');
    const fallback=auth.slice(auth.indexOf('public RW_AuthFallback('),auth.indexOf('stock RW_ArmFallback'));
    assert.ok(fallback.includes('gAuthPageReady[playerid]'));
    assert.ok(!fallback.includes('|| gCefReady[playerid]'));
    const mode=fs.readFileSync('server/gamemodes/railway.pwn','latin1');
    const progress=mode.slice(mode.indexOf('public OnCefDownloadProgress'),mode.indexOf('public OnCefDownloadFinish'));
    assert.ok(!progress.includes('SetTimerEx'));
    assert.ok(!mode.includes('KEY_LOOK_RIGHT'),'old GTA E/Look Right action must no longer toggle the engine');
    const commands=fs.readFileSync('server/pawno/include/railway/commands.inc','latin1');
    assert.ok(commands.includes('RW_EngineCommand(playerid)'),'text command remains available without CEF');
});
test('backslash binding: reset, label and physical Backslash use VK 220',()=>{
    const ui=boot('railway_control');
    ui.events['control:settings'](76,69,220);
    assert.equal(ui.elements.content.children[2].children[1].textContent,'\\');
    ui.elements.content.children[0].onclick();
    ui.domEvents.keydown({key:'\\',code:'Backslash',keyCode:220,preventDefault(){},stopPropagation(){}});
    // Cannot duplicate the menu binding.
    assert.notEqual(ui.emitted.at(-1)[0],'control:bind');
    ui.elements.content.children.at(-1).onclick();
    assert.deepEqual(Array.from(ui.emitted.at(-1)),['control:bind',76,221,220]);
});
test('CEF auth hides registration for existing accounts and submits login only',()=>{
    const ui=boot('railway_auth');
    ui.events['auth:init']('Existing_Player',true,'1.0');
    assert.equal(ui.elements.tabs.hidden,true);
    assert.equal(ui.elements['submit-label'].textContent,'Войти на вокзал');
    ui.elements.tabs.listeners.click({target:{closest(){return {dataset:{mode:'register'}};}}});
    assert.equal(ui.elements['submit-label'].textContent,'Войти на вокзал');
    ui.elements.pass.value='secret123';
    ui.elements.form.listeners.submit({preventDefault(){}});
    assert.deepEqual(Array.from(ui.emitted.at(-1)),['auth:login','secret123']);
});
test('CEF auth offers registration only for an absent SQLite account',()=>{
    const ui=boot('railway_auth');
    ui.events['auth:init']('New_Player',false,'1.0');
    assert.equal(ui.elements['submit-label'].textContent,'Создать аккаунт');
    ui.elements.pass.value='secret123'; ui.elements.pass2.value='secret123';
    ui.elements.form.listeners.submit({preventDefault(){}});
    assert.deepEqual(Array.from(ui.emitted.at(-1)),['auth:register','secret123','secret123']);
});
test('bridge waits for late CEF injection, registers handlers before ready and retries until ACK',()=>{
    const intervals=[], cleared=new Set(), window={}, calls=[], handlers={};
    const context={window,setInterval(fn){intervals.push(fn);return intervals.length-1;},clearInterval(id){cleared.add(id);}};
    vm.runInNewContext(fs.readFileSync('server/scriptfiles/cef/railway_control/bridge.js','utf8'),context);
    window.railwayCef.on('control:settings',()=>{});
    window.railwayCef.emit('control:ready');
    for(let i=0;i<150;i++) intervals[1](); // More than three minutes without native bridge.
    assert.equal(cleared.size,0,'late injection must still be accepted');
    window.cef={on(k,fn){handlers[k]=fn;calls.push(['on',k]);},emit(k){calls.push(['emit',k]);}};
    intervals[0]();
    assert.ok(calls.findIndex(v=>v[1]==='control:settings') < calls.findIndex(v=>v[0]==='emit'));
    intervals[1]();
    assert.equal(calls.filter(v=>v[0]==='emit').length,2);
    handlers['railway:ack'](); intervals[1]();
    assert.ok(cleared.has(1));
});
test('map teleport requires confirmation and revalidates state on response',()=>{
    const source=fs.readFileSync('server/gamemodes/railway.pwn','latin1');
    const click=source.slice(source.indexOf('public OnPlayerClickMap'),source.indexOf('public OnCefInitialize'));
    assert.ok(click.includes('gMapPending[playerid] = true'));
    assert.ok(!click.includes('SetPlayerPosFindZ'));
    const response=source.slice(source.indexOf('if (dialogid == DIALOG_MAP_TELEPORT)'),source.indexOf('public OnPlayerClickMap'));
    assert.ok(response.includes('!response || !gLogged[playerid]'));
    assert.ok(response.includes('PLAYER_STATE_ONFOOT'));
    assert.ok(response.includes('gMapPending[playerid] = false'));
});
test('late auth page is kept alive during fallback and can replace the native dialog',()=>{
    const source=fs.readFileSync('server/pawno/include/railway/accounts.inc','latin1');
    const fallback=source.slice(source.indexOf('stock RW_AuthFallbackNow'),source.indexOf('forward RW_AuthFallback'));
    assert.ok(!fallback.includes('CEF_DestroyBrowser'));
    const ready=source.slice(source.indexOf('public RW_OnAuthReady'),source.indexOf('public RW_OnAuthLogin'));
    assert.ok(ready.includes('ShowPlayerDialog(playerid, -1'));
    assert.ok(ready.includes('RW_UI_OpenAuth(playerid)'));
    assert.ok(ready.includes('if (gLogged[playerid]) return 1'));
});
test('engine default is physical BracketRight, displayed as Ъ / ], in both layouts',()=>{
    const ui=boot('railway_control');
    ui.events['control:settings'](76,221,220);
    assert.equal(ui.elements.content.children[1].children[1].textContent,'Ъ / ]');
    // Select another key first, then rebind through the physical code in Russian layout.
    ui.events['control:settings'](76,69,220);
    ui.elements.content.children[1].onclick();
    ui.domEvents.keydown({key:'ъ',code:'BracketRight',keyCode:0,preventDefault(){},stopPropagation(){}});
    assert.deepEqual(Array.from(ui.emitted.at(-1)),['control:bind',76,221,220]);
    const pawn=fs.readFileSync('server/pawno/include/railway/interaction.inc','latin1');
    assert.ok(pawn.includes('gControlKeys[playerid][1] = 221;'));
    assert.ok(pawn.includes('|| key == 221'));
});
test('settings and vehicle menu confirm actual opening, not just page readiness',()=>{
    const ui=boot('railway_control');
    assert.equal(ui.emitted.at(-1)[0],'control:ready');
    ui.events['control:settings'](76,221,220);
    assert.equal(ui.emitted.at(-1)[0],'control:opened');
    ui.events['control:vehicle']('Sultan',0,false);
    assert.equal(ui.emitted.at(-1)[0],'control:opened');
    ui.domEvents.keydown({key:'Escape',preventDefault(){}});
    assert.equal(ui.elements.panel.hidden,true);
    assert.equal(ui.emitted.at(-1)[0],'control:close');
});
test('menu uses model bounds, has bounded retries and frees capture after failure',()=>{
    const pawn=fs.readFileSync('server/pawno/include/railway/interaction.inc','latin1');
    const target=pawn.slice(pawn.indexOf('stock RW_ControlTarget'),pawn.indexOf('stock RW_ControlPublish'));
    assert.ok(target.includes('VEHICLE_MODEL_INFO_SIZE'));
    assert.ok(target.includes('GetVehicleZAngle'));
    assert.equal((target.match(/RW_RayAxis\(/g)||[]).length,3);
    assert.ok(target.includes('IsPlayerInRangeOfPoint'));
    const timeout=pawn.slice(pawn.indexOf('public RW_ControlTimeout'));
    assert.ok(timeout.includes('attempt >= 2'));
    assert.ok(timeout.includes('RW_ControlClose(playerid, CONTROL_BROWSER)'));
    assert.ok(timeout.includes('CEF_ReloadBrowser'));
    const mode=fs.readFileSync('server/gamemodes/railway.pwn','latin1');
    const disconnect=mode.slice(mode.indexOf('public OnPlayerDisconnect'),mode.indexOf('public OnPlayerRequestClass'));
    assert.ok(disconnect.includes('RW_ControlCancelWait'));
});
