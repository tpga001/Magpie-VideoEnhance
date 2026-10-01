const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const ctx = { ArrayBuffer, DataView, Uint8Array };
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../browser-extension/frame-packet.js'), 'utf8'), ctx);
function frame(format='NV12') {
  return { format, displayWidth:1920, displayHeight:1080, visibleRect:{x:0,y:0,width:1920,height:1080},
    colorSpace:{matrix:'bt709',transfer:'bt709',fullRange:false},
    async copyTo(buffer,options){this.options=options;buffer.fill(128);} };
}
test('1080p NV12 remains 1080p with exact plane layout and source timestamp',async()=>{
  const f=frame(), result=await ctx.MagpieReadFrame(f,{x:0,y:0,width:1920,height:1080},456789);
  const h=new DataView(result.packet);
  assert.equal(result.packet.byteLength,64+1920*1080*3/2);
  assert.equal(h.getUint32(8,true),1920);assert.equal(h.getUint32(12,true),1080);
  assert.equal(h.getUint32(48,true),1);assert.equal(h.getBigInt64(40,true),456789n);
  assert.equal(f.options.layout[1].offset,1920*1080);assert.equal(f.options.layout[1].stride,1920);
  assert.equal(new Uint8Array(result.packet)[64],128);
});
test('odd crop uses RGBA without incorrectly shifting YUV chroma',async()=>{
  const f=frame(),result=await ctx.MagpieReadFrame(f,{x:1,y:0,width:1919,height:1080},0);
  assert.equal(result.format,'RGBA');assert.equal(f.options.format,'RGBA');assert.equal(f.options.rect.x,1);
  assert.equal(new DataView(result.packet).getUint32(16,true),1919*4);
});
test('viewport clipping stays on NV12 and avoids a full RGBA readback',async()=>{
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../browser-extension/video-geometry.js'),'utf8'),ctx);
  const geometry=ctx.MagpieVideoGeometry({videoWidth:1920,videoHeight:1080,
    getBoundingClientRect:()=>({left:0,top:0,right:3840,bottom:2160,width:3840,height:2160})},
    {innerWidth:3840,innerHeight:2159},{});
  const f=frame(),result=await ctx.MagpieReadFrame(f,geometry.crop,123);
  assert.equal(result.format,'NV12');assert.equal(result.height,1078);
  assert.equal(result.packet.byteLength,64+1920*1078*3/2);
  assert.equal(f.options.format,undefined,'do not request RGBA conversion');
  assert.equal(f.options.rect.height,1078);
});
test('odd decoder-visible origin still uses the safe RGBA fallback',async()=>{
  const f=frame();f.visibleRect.x=1;
  const result=await ctx.MagpieReadFrame(f,{x:0,y:0,width:1920,height:1080},0);
  assert.equal(result.format,'RGBA');assert.equal(f.options.rect.x,1);
});
test('HDR is rejected before readback and mismatched buffers are not reused',async()=>{
  const f=frame();f.colorSpace.transfer='pq';
  await assert.rejects(ctx.MagpieReadFrame(f,{x:0,y:0,width:1920,height:1080},0),/SDR/);
  const small=new ArrayBuffer(64),result=await ctx.MagpieReadFrame(frame(),{x:0,y:0,width:1920,height:1080},0,small);
  assert.notEqual(result.packet,small);
});
test('worker caps in-flight frames, reports only accepted frames and resets timeout after pause',()=>{
  const messages=[],timers=new Map();let next=1,socket;
  class Socket {
    static OPEN=1;
    constructor(){socket=this;this.readyState=1;this.bufferedAmount=0;this.sent=[];}
    send(value){this.sent.push(value);} close(){this.readyState=3;}
  }
  const worker={WebSocket:Socket,Blob,performance:{now:()=>1000},postMessage:message=>messages.push(message),
    setTimeout:(fn,delay)=>{const id=next++;timers.set(id,{fn,delay});return id;},clearTimeout:id=>timers.delete(id)};
  vm.createContext(worker);vm.runInContext(fs.readFileSync(path.join(__dirname,'../browser-extension/frame-worker.js'),'utf8'),worker);
  worker.onmessage({data:{type:'init',connection:{url:'ws://127.0.0.1:1234/'+ 'A'.repeat(64)}}});socket.onopen();
  for(let i=0;i<4;i++)worker.onmessage({data:{type:'frame',packet:new ArrayBuffer(64),width:1920,height:1080}});
  assert.equal(socket.sent.length,3);assert.equal(messages.filter(m=>m.type==='queued').length,4);
  assert.equal(messages.at(-1).stats.droppedQueue,1);assert.equal(messages.at(-1).stats.droppedBackpressure,0);
  socket.onmessage({data:'{"ok":true,"dropped":true}'});
  assert.equal(messages.filter(m=>m.type==='ack').length,0);
  assert.equal(messages.at(-1).type,'stats');assert.equal(messages.at(-1).stats.droppedHost,1);
  socket.onmessage({data:'{"ok":true}'});socket.onmessage({data:'{"ok":true}'});
  assert.equal(timers.size,0,'no stale timeout while paused');
  socket.bufferedAmount=9*1024*1024;
  worker.onmessage({data:{type:'frame',packet:new ArrayBuffer(64),width:1280,height:720}});
  assert.equal(messages.at(-1).stats.droppedBackpressure,1);
  socket.bufferedAmount=0;
  worker.onmessage({data:{type:'frame',packet:new ArrayBuffer(64),width:1280,height:720}});
  assert.equal(timers.size,1);socket.onmessage({data:'{"ok":true}'});
  assert.equal(messages.at(-1).width,1280);assert.equal(messages.at(-1).stats.acks,4);assert.equal(timers.size,0);
});
