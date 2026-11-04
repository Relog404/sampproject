// No dependencies: exercise the actual CEF scripts with a small DOM/event stub.
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
function boot(resource) {
    const elements = {}, events = {}, emitted = [], timers = [];
    function element() {
        return {style:{setProperty(){}}, children:[], hidden:true, attrs:{}, textContent:'',
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
        setTimeout(fn,ms){timers.push({fn,ms});}, requestAnimationFrame(fn){frame=fn;}, console};
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
    const nativeKeys=mode.slice(mode.indexOf('public OnPlayerKeyStateChange'));
    assert.ok(nativeKeys.includes('gCefReady[playerid] ||'));
    assert.ok(nativeKeys.includes('KEY_LOOK_RIGHT'));
});
