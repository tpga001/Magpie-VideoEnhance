const { spawn } = require('node:child_process');
const net = require('node:net');
const child = spawn(process.env.FRAME_HOST, [], { windowsHide:true });
let stdout=Buffer.alloc(0), count=0, started, socket;
const request=Buffer.from(JSON.stringify({action:'frames',pageUrl:'http://localhost:9999/'}));
const prefix=Buffer.alloc(4);prefix.writeUInt32LE(request.length);child.stdin.write(Buffer.concat([prefix,request]));
const timeout=setTimeout(()=>{socket?.destroy();child.kill();throw new Error('benchmark timed out');},20000);
child.stdout.on('data',chunk=>{
  stdout=Buffer.concat([stdout,chunk]); if(stdout.length<4||stdout.length<stdout.readUInt32LE(0)+4)return;
  const reply=JSON.parse(stdout.subarray(4)); if(!reply.ok||!reply.url)throw new Error(JSON.stringify(reply));
  const url=new URL(reply.url);
  socket=net.createConnection({host:'127.0.0.1',port:Number(url.port)});
  let response=Buffer.alloc(0),upgraded=false;
  const payload=Buffer.alloc(64+1920*1080*3/2);
  [0x31465642,1,1920,1080,1920,1920*1080*3/2].forEach((n,i)=>payload.writeUInt32LE(n,i*4));
  payload.writeUInt32LE(1,48);payload.writeUInt32LE(1,52);
  const wsHeader=Buffer.alloc(14);wsHeader[0]=0x82;wsHeader[1]=0xff;wsHeader.writeBigUInt64BE(BigInt(payload.length),2);
  const frame=Buffer.concat([wsHeader,payload]);
  socket.on('connect',()=>socket.write(`GET ${url.pathname} HTTP/1.1\r\nHost: 127.0.0.1:${url.port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nOrigin: http://localhost:9999\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: AAECAwQFBgcICQoLDA0ODw==\r\n\r\n`));
  socket.on('data',chunk=>{
    response=Buffer.concat([response,chunk]);
    if(!upgraded){const end=response.indexOf('\r\n\r\n');if(end<0)return;response=response.subarray(end+4);upgraded=true;started=performance.now();socket.write(frame);}
    while(response.length>=2){let length=response[1]&127,header=2;if(length===126){if(response.length<4)return;length=response.readUInt16BE(2);header=4;}if(response.length<header+length)return;
      const ack=JSON.parse(response.subarray(header,header+length));response=response.subarray(header+length);
      if(++count===60){console.log(JSON.stringify({frames:count,ms:performance.now()-started,ack}));clearTimeout(timeout);socket.destroy();child.stdin.end();return;}
      socket.write(frame);
    }
  });
});
