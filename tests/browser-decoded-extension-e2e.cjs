// Real unpacked extension in a disposable Edge profile; no extension API shim.
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { execFileSync, spawn } = require('node:child_process');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const runtime = process.env.MAGPIE_RUNTIME;
const browserExecutable = process.env.MAGPIE_TEST_BROWSER_EXECUTABLE || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
const browserName = process.env.MAGPIE_TEST_BROWSER_NAME || 'Edge';
const focusOnly = process.env.MAGPIE_FOCUS_ONLY === '1';
const output = path.resolve(__dirname, '../output/playwright/browser-decoded');
const extension = path.join(output, 'edge-extension');
const checks=[],focusTrace=[];
fs.mkdirSync(extension, { recursive:true });
for(const file of fs.readdirSync(path.join(__dirname,'../browser-extension'))) {
  let data=fs.readFileSync(path.join(__dirname,'../browser-extension',file));
  if(file==='service-worker.js')data=Buffer.from(data.toString().replace('com.magpie.browser_video','com.magpie.browser_video_decoded_test').replace(
    'const activeTab = await focusedActiveTab();',
    'const activeTab = await focusedActiveTab(); globalThis.testReport = { time:Date.now(), activeTabId:activeTab?.id, senderTabId:sender.tab.id, rect:message.candidate?.rect, source:message.candidate?.source };').replace(
    'async function sendNativeNow(message) {','async function sendNativeNow(message) { if(message.action === "update") globalThis.testLastNative = message;'));
  fs.writeFileSync(path.join(extension,file),data);
}
const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const startedAt=new Date(Date.now()-new Date().getTimezoneOffset()*60000).toISOString().replace('T',' ').slice(0,19);
const log=()=>['magpie.1.log','magpie.log'].map(name=>{
  const file=path.join(runtime,'logs',name);return fs.existsSync(file)?fs.readFileSync(file,'utf8'):'';
}).join('\n').split('\n').filter(line=>line.slice(0,19)>=startedAt).join('\n');
function control(action) {
  return JSON.parse(execFileSync('pwsh.exe',['-NoProfile','-File',path.join(__dirname,'BrowserVideoSmokeControl.ps1'),'-Action',action,'-RuntimeDirectory',runtime],{encoding:'utf8',windowsHide:true,timeout:20000}));
}
function windowState(action='inspect') {
  return JSON.parse(execFileSync('pwsh.exe',['-NoProfile','-File',path.join(__dirname,'BrowserFocusWindow.ps1'),'-Action',action],{encoding:'utf8',windowsHide:true,timeout:15000}));
}
(async()=>{
  let context,server,focusApp;
  const events=[];
  try {
    server=http.createServer((req,res)=>{res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'});res.end(req.url==='/blank'?'<title>Magpie foreground blank tab</title><p>No video on this page</p>':fs.readFileSync(path.join(__dirname,'browser-decoded-1080p30.html')));});
    await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
    context=await chromium.launchPersistentContext(path.join(output,'edge-profile-'+Date.now()),{
      executablePath:browserExecutable,headless:false,viewport:null,
      args:[`--disable-extensions-except=${extension}`,`--load-extension=${extension}`,'--window-size=1100,800','--window-position=80,80','--autoplay-policy=no-user-gesture-required']
    });
    const page=context.pages()[0]||await context.newPage();
    context.on('console',message=>events.push(message.text().replace(/ws:\/\/127\.0\.0\.1:\d+\/[A-F0-9]+/g,'[local frame endpoint]')));
    page.on('pageerror',error=>events.push(error.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/`,{waitUntil:'domcontentloaded'});
    // The fixture encodes its local clip before assigning video.src. Browser
    // load can wait for that media work, so observe the explicit ready signal.
    await page.waitForFunction(()=>document.title.includes('[READY]'),null,{timeout:60000});
    console.log('local 1080p30 video fixture is ready');
    let worker=context.serviceWorkers().find(worker=>worker.url().includes('peflachlahjaaldgfbjkiniphnplmhgp'));
    if(!worker)worker=await context.waitForEvent('serviceworker',{timeout:15000});
    await worker.evaluate(()=>{
      globalThis.testFocusEvents=[];
      chrome.windows.onFocusChanged.addListener(windowId=>globalThis.testFocusEvents.push({time:Date.now(),windowId}));
    });
    await worker.evaluate(async()=>chrome.storage.local.set({enabled:false}));
    await wait(1000);
    const before=log().length;
    await worker.evaluate(async()=>chrome.storage.local.set({enabled:true,lastError:'',statusNote:''}));
    await wait(2000);
    await page.bringToFront();
    focusTrace.push({time:Date.now(),stage:'initial activation',...windowState('focusBrowser')});
    let state;
    const end=Date.now()+45000;
    while(Date.now()<end){
      state=await worker.evaluate(()=>chrome.storage.local.get(null));
      const observed=windowState();
      const latest=focusTrace.at(-1);
      if(observed.foreground!==latest?.foreground||observed.overlayVisible!==latest?.overlayVisible)focusTrace.push({time:Date.now(),stage:'initial wait',...observed});
      if(!observed.foregroundTitle.startsWith('Magpie Decoded 1080p30')){
        windowState('focusBrowser');await wait(250);continue;
      }
      if(state.statusNote?.startsWith('原始帧增强')&&(focusOnly?windowState().overlayVisible:log().slice(before).includes('RTX Video VSR Draw succeeded')))break;
      if(state.lastError||state.statusNote?.includes('失败')||state.statusNote?.includes('超时')||state.statusNote?.includes('未获授权'))throw new Error(JSON.stringify(state));
      await wait(500);
    }
    if(!state?.statusNote?.startsWith('原始帧增强')||!(focusOnly?windowState().overlayVisible:log().slice(before).includes('RTX Video VSR Draw succeeded')))throw new Error(JSON.stringify({state,report:await worker.evaluate(()=>globalThis.testReport),focusEvents:await worker.evaluate(()=>globalThis.testFocusEvents),focusTrace}));
    const until=async(check,name)=>{
      const end=Date.now()+45000;
      while(Date.now()<end){if(await check()){checks.push(name);console.log(name);return;}await wait(500);}
      throw new Error(name+' timed out: '+JSON.stringify({window:windowState(),browser:await worker.evaluate(async()=>({state:await chrome.storage.local.get(null),report:globalThis.testReport,focused:await chrome.windows.getLastFocused(),focusEvents:globalThis.testFocusEvents}))}));
    };
    if(!focusOnly)await until(()=>log().slice(before).includes('Browser decoded frames uploaded: frames=300'),'300 original frames processed through VSR/FG');
    const foregroundState=windowState();
    if(!foregroundState.overlayVisible||foregroundState.overlayTopmost||foregroundState.toastVisible)throw new Error('Unexpected foreground presentation: '+JSON.stringify(foregroundState));
    checks.push('foreground enhancement is visible, non-topmost and silent');
    await page.evaluate(()=>{
      const text=document.createElement('div');text.className='bpx-player-subtitle-panel';
      text.style.cssText='position:fixed;left:40%;top:45%;width:20%;height:10%;background:#102030;color:white;font:24px sans-serif;z-index:50';
      text.textContent='字幕和进度条保持原样';document.body.append(text);
    });
    await wait(1500);
    const overlay=JSON.parse(execFileSync('pwsh.exe',['-NoProfile','-File',path.join(__dirname,'Inspect-BrowserVideoOverlay.ps1'),'-OutputPath',path.join(output,'real-overlay.png')],{encoding:'utf8',windowsHide:true}));
    if(overlay.regionKind!==3||overlay.holeCovered)throw new Error('Subtitle masking failed: '+JSON.stringify(overlay));
    checks.push('DOM subtitle region stays outside enhancement');
    const cdp=await context.newCDPSession(page);
    const {windowId}=await cdp.send('Browser.getWindowForTarget');
    let offset=log().length;
    await cdp.send('Browser.setWindowBounds',{windowId,bounds:{width:850,height:650,windowState:'normal'}});
    await page.bringToFront();
    windowState('focusBrowser');
    await until(()=>focusOnly?windowState().overlayVisible:log().slice(offset).includes('Browser decoded source initialized: 1920x1080')&&log().slice(offset).includes('RTX Video VSR Draw succeeded'),'resized video enhancement resumes');
    offset=log().length;
    await page.evaluate(()=>document.querySelector('video').pause());
    await until(()=>focusOnly?!windowState().overlayVisible:log().slice(offset).includes('Browser video auto mode stopped'),'pause removes overlay');
    offset=log().length;
    await page.evaluate(()=>document.querySelector('video').play());
    await until(()=>focusOnly?windowState().overlayVisible:log().slice(offset).includes('RTX Video VSR Draw succeeded'),'resume restores enhancement');

    focusApp=spawn('pwsh.exe',['-NoProfile','-STA','-File',path.join(__dirname,'BrowserFocusWindow.ps1'),'-Action','app'],{windowsHide:true,stdio:['ignore','pipe','pipe']});
    let fixtureError='';
    focusApp.stderr.on('data',data=>{fixtureError+=data.toString();});
    focusApp.stdout.on('data',data=>{events.push(data.toString());});
    await until(()=>{
      if(focusApp.exitCode!==null)throw new Error('Focus fixture exited: '+fixtureError);
      return windowState().fixtureVisible;
    },'foreground test application is ready');
    // Creating a WinForms fixture can briefly activate it before its
    // SW_SHOWNOACTIVATE correction runs. Establish a stable browser baseline
    // before measuring the intentional 150 ms focus pulse.
    windowState('focusBrowser');
    await until(()=>windowState().overlayVisible,'browser baseline before transient focus pulse');
    const transientOffset=log().length;
    windowState('pulse');await wait(800);
    const transientState=windowState();
    const transientLog=log().slice(transientOffset);
    if(!transientState.overlayVisible||/Browser video overlay suspended|Diagnostic MP-011/.test(transientLog))throw new Error(
      'Transient focus pulse rebuilt enhancement: '+JSON.stringify({transientState,transientLog}));
    checks.push('brief focus pulse keeps the decoded-frame mapping and overlay');
    windowState('focusApp');
    await until(()=>!windowState().overlayVisible,'switching applications removes the video overlay');
    // Simulate reports already queued before the user switched applications.
    // This exercises the native guard independently of extension focus events.
    for(let i=0;i<3;i++){
      await worker.evaluate(()=>chrome.runtime.sendNativeMessage('com.magpie.browser_video_decoded_test',globalThis.testLastNative));
      await wait(700);
      const background=windowState();
      if(background.overlayVisible||background.overlayTopmost||!background.foregroundTitle.startsWith('Magpie Focus Regression App'))throw new Error('Background update stole presentation: '+JSON.stringify(background));
    }
    checks.push('late native video reports cannot cover another application');
    offset=log().length;windowState('focusBrowser');
    await until(()=>windowState().overlayVisible,'returning to video resumes enhancement');
    const blank=await context.newPage();
    await blank.goto(`http://127.0.0.1:${server.address().port}/blank`);
    await blank.bringToFront();
    await until(()=>!windowState().overlayVisible,'switching tabs removes the old video overlay');
    await wait(2500);
    if(windowState().overlayVisible)throw new Error('Background tab reactivated its overlay');
    await blank.close();await page.bringToFront();windowState('focusBrowser');
    await until(()=>windowState().overlayVisible,'returning to video tab resumes enhancement');

    await worker.evaluate(()=>chrome.storage.local.set({enabled:false}));await wait(1000);
    await worker.evaluate(()=>chrome.storage.local.set({enabled:true}));
    await wait(150);windowState('focusApp');await wait(5000);
    const duringStart=windowState();
    if(duringStart.overlayVisible||duringStart.overlayTopmost||!duringStart.foregroundTitle.startsWith('Magpie Focus Regression App'))throw new Error('In-flight startup stole presentation: '+JSON.stringify(duringStart));
    checks.push('switching away during startup cannot show a delayed overlay');
    windowState('focusBrowser');await until(()=>windowState().overlayVisible,'foreground return after cancelled startup recovers');
    if(log().slice(before).includes('Diagnostic MP-011'))throw new Error('Browser decoded-frame validation produced MP-011');
    checks.push('focus, resize and restart validation produced no MP-011');
    control('quit');
    await until(async()=>!(await worker.evaluate(()=>chrome.storage.local.get('enabled'))).enabled,'tray exit turns enhancement off');
    await wait(3000);
    if(control('state').running)throw new Error('Magpie restarted after exit');
    checks.push('Magpie stays closed after exit');
    const result={status:'passed',browser:browserName,extensionVersion:JSON.parse(fs.readFileSync(path.join(extension,'manifest.json'))).version,state,checks,focusTrace};
    fs.writeFileSync(path.join(output,'extension-runtime.log'),log().slice(before));
    fs.writeFileSync(path.join(output,'extension-result.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
    await worker.evaluate(async()=>chrome.storage.local.set({enabled:false}));
  }finally{fs.writeFileSync(path.join(output,'extension-console.json'),JSON.stringify(events,null,2));focusApp?.kill();control('quit');await context?.close();server?.close();}
})().catch(error=>{fs.writeFileSync(path.join(output,'extension-result.json'),JSON.stringify({status:'failed',error:error.message,checks,focusTrace},null,2));console.error(error);process.exitCode=1;});
