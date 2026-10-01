// Bounded, real-window validation. No production API or focus-state mocks.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {spawn,execFileSync}=require('node:child_process');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const runtime=process.env.MAGPIE_RUNTIME,output=process.env.MAGPIE_QUALITY_OUTPUT;
const geometryOnly=process.env.MAGPIE_GEOMETRY_ONLY==='1';
const useSidePanelMeasurement=process.env.MAGPIE_USE_SIDEPANEL_MEASUREMENT==='1';
const sidePanelSessionPath='E:/LocalConfig/performance-panel/fps-session.xml';
if(!runtime||!output)throw new Error('Runtime and output directory are required');
function integerEnv(name,fallback,min,max){
 const raw=process.env[name];if(raw==null||raw.trim()==='')return fallback;
 const value=Number(raw);if(!Number.isInteger(value)||value<min||value>max)throw new Error(`${name} must be an integer from ${min} to ${max}`);
 return value;
}
function displayId(value){const match=String(value).trim().toUpperCase().match(/DISPLAY\d+$/);return match?.[0]||'';}
const captureSeconds=integerEnv('MAGPIE_QUALITY_SECONDS',25,1,360);
const requestedDisplays=(process.env.MAGPIE_QUALITY_DISPLAYS||'DISPLAY1,DISPLAY2').split(',').map(value=>{
 if(!value.trim())throw new Error('MAGPIE_QUALITY_DISPLAYS contains an empty display');
 const id=displayId(value);if(!id)throw new Error(`Invalid display identifier: ${value}`);return id;
});
if(new Set(requestedDisplays).size!==requestedDisplays.length)throw new Error('MAGPIE_QUALITY_DISPLAYS contains a duplicate display');
fs.mkdirSync(output,{recursive:true});
const extension=path.join(output,'extension');fs.mkdirSync(extension,{recursive:true});
for(const name of fs.readdirSync(path.join(__dirname,'../browser-extension'))){
 let data=fs.readFileSync(path.join(__dirname,'../browser-extension',name));
 if(name==='service-worker.js')data=Buffer.from(data.toString().replace('com.magpie.browser_video','com.magpie.browser_video_decoded_test'));
 if(name==='content.js')data=Buffer.from(data.toString().replace('  function clippedRect(element) {',
  '  chrome.runtime.onMessage.addListener(message => { if(message.type === "magpie-test-stats") return Promise.resolve(frames?.stats || null); });\n  function clippedRect(element) {'));
 fs.writeFileSync(path.join(extension,name),data);
}
const startedAt=new Date(Date.now()-new Date().getTimezoneOffset()*60000).toISOString().replace('T',' ').slice(0,19);
const result={status:'running',startedAt,runtime,buildCommit:JSON.parse(fs.readFileSync(path.join(runtime,'build-manifest.json'))).commit,captureSeconds,requestedDisplays,measurementProvider:useSidePanelMeasurement?'SidePanel single PresentMon session':'dedicated PresentMon',stages:[]};
const sidePanelBaselineMtime=useSidePanelMeasurement&&fs.existsSync(sidePanelSessionPath)?fs.statSync(sidePanelSessionPath).mtimeMs:0;
const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function ps(script,args=[]){return JSON.parse(execFileSync('pwsh.exe',['-NoProfile','-File',path.join(__dirname,script),...args],{encoding:'utf8',windowsHide:true,timeout:20000}));}
const focus=()=>ps('BrowserFocusWindow.ps1',['-Action','focusBrowser']);
// Windows can reject SetForegroundWindow transiently. Setup may retry, while
// the timed observation below still treats any focus loss as a hard failure.
const tryFocus=()=>{try{return focus();}catch{return null;}};
const probe=(action='state',args=[])=>ps('BrowserVideoDisplayProbe.ps1',['-Action',action,...args]);
const control=action=>ps('BrowserVideoSmokeControl.ps1',['-Action',action,'-RuntimeDirectory',runtime]);
function asyncTool(exe,args,timeout=60000){
 return new Promise((resolve,reject)=>{
  const child=spawn(exe,args,{windowsHide:true,stdio:['ignore','pipe','pipe']});let stdout='',stderr='';
  child.stdout.on('data',d=>stdout+=d);child.stderr.on('data',d=>stderr+=d);
  const timer=setTimeout(()=>{child.kill();reject(new Error('Timed out: '+exe));},timeout);
  child.on('error',error=>{clearTimeout(timer);reject(error);});
  child.on('close',code=>{clearTimeout(timer);resolve({code,stdout,stderr});});
 });
}
const save=()=>fs.writeFileSync(path.join(output,'result.json'),JSON.stringify(result,null,2));
(async()=>{
 let context,server,worker;
 try{
  result.monitors=probe('monitors');
  const targets=requestedDisplays.map(id=>result.monitors.find(m=>displayId(m.Device)===id));
  if(targets.some(m=>!m))throw new Error(`Requested monitor is not connected: ${requestedDisplays[targets.findIndex(m=>!m)]}`);
  server=http.createServer((req,res)=>{res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'});res.end(fs.readFileSync(path.join(__dirname,'browser-decoded-1080p30.html')));});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  context=await chromium.launchPersistentContext(path.join(output,'edge-profile'),{
   executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:false,viewport:null,
   args:[`--disable-extensions-except=${extension}`,`--load-extension=${extension}`,'--window-size=1100,800','--window-position=100,80','--autoplay-policy=no-user-gesture-required']
  });
  const page=context.pages()[0];
  await page.goto(`http://127.0.0.1:${server.address().port}/?duration=${Math.min(390,captureSeconds+120)}`,{waitUntil:'domcontentloaded',timeout:60000});
  await page.waitForFunction(()=>document.title.includes('[READY]'),null,{timeout:60000});
  worker=context.serviceWorkers().find(w=>w.url().includes('peflachlahjaaldgfbjkiniphnplmhgp'))||await context.waitForEvent('serviceworker');
  await worker.evaluate(()=>chrome.storage.local.set({enabled:false}));
  const state=()=>worker.evaluate(()=>chrome.storage.local.get(null));
  const until=async(check,label)=>{const end=Date.now()+45000;while(Date.now()<end){if(await check())return;const s=await state();if(s.lastError)throw new Error(s.lastError);await wait(500);}throw new Error(label+': '+JSON.stringify({state:await state(),window:probe()}));};
  await page.evaluate(()=>{
   const ui=document.createElement('div');ui.className='bpx-player-subtitle-panel';ui.style.cssText='position:fixed;left:35%;top:65%;color:white;background:#102030;font:24px sans-serif;z-index:50';ui.textContent='网页字幕保持原样';document.body.append(ui);
   window.videoFrames=[];const sample=(time,meta)=>{window.videoFrames.push({time,mediaTime:meta.mediaTime,presented:meta.presentedFrames});document.querySelector('video').requestVideoFrameCallback(sample);};document.querySelector('video').requestVideoFrameCallback(sample);
  });
  for(let index=0;index<targets.length;index++){
    const monitor=targets[index],id=displayId(monitor.Device),name=id==='DISPLAY1'?'4k':id==='DISPLAY2'?'1080p-portrait':id.toLowerCase();
    let setupFocusRecoveries=0;
    console.log('Moving video to '+name);
    probe('move',['-Device',monitor.Device]);await page.bringToFront();tryFocus();await wait(2500);
    if(index===0){
     probe('capture',['-OutputPath',path.join(output,name+'-off.png')]);
     await worker.evaluate(()=>chrome.storage.local.set({enabled:true}));
    }
    // Runtime/native-host startup and test helpers can briefly take foreground.
    // Recover only during setup; the timed observation below treats any focus
    // loss as a product failure.
    await until(()=>{
     const s=probe();
      if(s.foreground!==s.browser){tryFocus();setupFocusRecoveries++;return false;}
     return s.monitor===monitor.Device&&s.overlayVisible&&s.overlayRect&&s.overlayRect.Left>=monitor.Monitor.Left&&s.overlayRect.Right<=monitor.Monitor.Right;
    },'enhancement on destination display');
    await wait(geometryOnly?2000:6000);
    const stage={name,monitor,startedAt:new Date().toISOString(),setupFocusRecoveries,initial:probe(),state:await state()};result.stages.push(stage);save();
   const pid=control('state').pids[0];if(!pid)throw new Error('Test runtime is not running');
   stage.pid=pid;
   const presentmonPath=path.join(output,name+'-presentmon.csv');
   const useDedicatedPresentMon=!geometryOnly&&!useSidePanelMeasurement&&!process.env.MAGPIE_EXTERNAL_PRESENTMON;
   let dedicatedCapture;
   if(useDedicatedPresentMon){
    const statusPath=presentmonPath+'.status.json';
    for(const suffix of ['', '.status.json', '.stdout.txt', '.stderr.txt']){
     const file=presentmonPath+suffix;if(fs.existsSync(file))fs.unlinkSync(file);
    }
    stage.captureArmStartedAt=new Date().toISOString();save();
    dedicatedCapture=asyncTool('pwsh.exe',['-NoProfile','-File',path.join(__dirname,'Capture-BrowserVideoPresentation.ps1'),'-OutputPath',presentmonPath,'-Seconds',String(Math.min(360,captureSeconds+30)),'-TargetProcessId',String(pid)],(captureSeconds+90)*1000);
    const armDeadline=Date.now()+30000;
    while(Date.now()<armDeadline){
     if(fs.existsSync(statusPath)){
      const captureStatus=JSON.parse(fs.readFileSync(statusPath,'utf8'));
      if(captureStatus.state==='running'){stage.captureArmed=captureStatus;break;}
     }
     await wait(250);
    }
    if(!stage.captureArmed)throw new Error('PresentMon did not reach running state before measurement');
    // Elevation can switch to the secure desktop and suspend the overlay. Arm
    // capture first, then restore the browser and warm the complete pipeline.
    await page.bringToFront();tryFocus();
    await until(()=>{const s=probe();return s.foreground===s.browser&&s.monitor===monitor.Device&&s.overlayVisible;},'foreground recovery after PresentMon elevation');
    await wait(10000);
   }
   // Screenshot/burst helpers and another desktop browser can take foreground
   // between setup and capture. Re-establish the controlled browser baseline
   // immediately before the bounded observation.
    await page.bringToFront();tryFocus();
   await until(()=>{const s=probe();return s.foreground===s.browser&&s.monitor===monitor.Device&&s.overlayVisible;},'foreground baseline before measurement');
   stage.preCapture=probe();
   stage.before=await page.evaluate(()=>{const q=document.querySelector('video').getVideoPlaybackQuality();return {quality:{totalVideoFrames:q.totalVideoFrames,droppedVideoFrames:q.droppedVideoFrames},time:performance.now()};});
   stage.relayBefore=await worker.evaluate(async()=>{const [tab]=await chrome.tabs.query({active:true,lastFocusedWindow:true});return chrome.tabs.sendMessage(tab.id,{type:'magpie-test-stats'});});
   await page.evaluate(()=>window.videoFrames=[]);
   stage.captureStartedAt=new Date().toISOString();
   const observeSeconds=geometryOnly?4:captureSeconds+1;
   console.log(geometryOnly?'Checking destination geometry on '+name:`Capturing ${captureSeconds} seconds of display events on ${name}`);
   const observation=await asyncTool('pwsh.exe',['-NoProfile','-File',path.join(__dirname,'BrowserVideoDisplayProbe.ps1'),'-Action','observe','-Seconds',String(observeSeconds),'-OutputPath',path.join(output,name+'-windows.json')],(observeSeconds+15)*1000);
   stage.captureEndedAt=new Date().toISOString();
   const capture=geometryOnly ? {code:0,notCollected:'geometry-only run'} : useSidePanelMeasurement ? {code:0,provider:'SidePanel'} : process.env.MAGPIE_EXTERNAL_PRESENTMON ? {code:0,externalCsv:process.env.MAGPIE_EXTERNAL_PRESENTMON} : await dedicatedCapture;
   stage.presentmon=capture;stage.observation=observation.code===0?JSON.parse(observation.stdout):observation;
   stage.after=await page.evaluate(()=>{const q=document.querySelector('video').getVideoPlaybackQuality();return {quality:{totalVideoFrames:q.totalVideoFrames,droppedVideoFrames:q.droppedVideoFrames},time:performance.now(),frames:window.videoFrames};});
   stage.relayAfter=await worker.evaluate(async()=>{const [tab]=await chrome.tabs.query({active:true,lastFocusedWindow:true});return chrome.tabs.sendMessage(tab.id,{type:'magpie-test-stats'});});
   stage.final=probe();stage.endedAt=new Date().toISOString();save();
   if(capture.code!==0){
    if((capture.stderr+capture.stdout).includes('access denied')){stage.displayMeasurement='unavailable: Windows ETW requires administrative permission';console.log(stage.displayMeasurement);}
    else throw new Error('PresentMon failed: '+capture.stderr+capture.stdout);
   }
    if(!geometryOnly&&!useSidePanelMeasurement&&!process.env.MAGPIE_EXTERNAL_PRESENTMON&&!fs.existsSync(presentmonPath))throw new Error(
    'PresentMon completed without a CSV: '+fs.readFileSync(presentmonPath+'.stderr.txt','utf8'));
   if(stage.observation.foregroundLoss||stage.observation.overlayMissing)throw new Error('Foreground/overlay interrupted during measurement');
   probe('capture',['-OutputPath',path.join(output,name+'-on.png')]);
   probe('burst',['-OutputPath',path.join(output,name+'-text')]);
   if(index===targets.length-1){await worker.evaluate(()=>chrome.storage.local.set({enabled:false}));await until(()=>!probe().overlayVisible,'disable enhancement');probe('capture',['-OutputPath',path.join(output,name+'-off.png')]);}
   console.log('Measurement completed on '+name);
  }
  result.status=geometryOnly?'geometry-and-screenshots-captured':result.stages.some(stage=>stage.displayMeasurement?.startsWith('unavailable'))?'captured-with-display-unavailable':'captured';
 }catch(error){result.status='failed';result.error=error.stack;process.exitCode=1;console.error(error);}
 finally{
  if(worker)await worker.evaluate(()=>chrome.storage.local.set({enabled:false})).catch(()=>{});
  try{control('quit');}catch(error){result.cleanupError=error.message;}
  if(useSidePanelMeasurement){
   const deadline=Date.now()+10000;
   while(Date.now()<deadline){
    if(fs.existsSync(sidePanelSessionPath)&&fs.statSync(sidePanelSessionPath).mtimeMs>sidePanelBaselineMtime){
     result.sidePanelSession={path:sidePanelSessionPath,modifiedAt:fs.statSync(sidePanelSessionPath).mtime.toISOString(),xml:fs.readFileSync(sidePanelSessionPath,'utf8')};
     break;
    }
    await wait(500);
   }
   if(!result.sidePanelSession&&!result.error){result.status='failed';result.error='SidePanel did not archive the Magpie display session';process.exitCode=1;}
  }
  await context?.close();server?.close();
  const logs=['magpie.1.log','magpie.log'].map(name=>{const f=path.join(runtime,'logs',name);return fs.existsSync(f)?fs.readFileSync(f,'utf8'):'';}).join('\n').split('\n').filter(line=>line.slice(0,19)>=startedAt).join('\n');
  fs.writeFileSync(path.join(output,'runtime.log'),logs);save();console.log(JSON.stringify({status:result.status,output,error:result.error}));
 }
})();
