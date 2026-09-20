import fs from 'node:fs';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {createHash} from 'node:crypto';
const run=promisify(execFile),dir='native/vendor';fs.mkdirSync(dir,{recursive:true});
async function download(url,file,hash){if(!fs.existsSync(file))await run('curl.exe',['-sSL','--fail','--retry','3','--max-time','600',url,'-o',file],{windowsHide:true});if(hash&&createHash('sha256').update(fs.readFileSync(file)).digest('hex')!==hash)throw Error('Checksum mismatch: '+file);}
if(!fs.existsSync(dir+'/raylib-6.0_win64_mingw-w64/include/raylib.h')){
 await download('https://github.com/raysan5/raylib/releases/download/6.0/raylib-6.0_win64_mingw-w64.zip',dir+'/raylib.zip','69688e025812c8132634c609c30938eda4a3fa14d63c4031108873a4d797e2d3');
 await run('tar.exe',['-xf',dir+'/raylib.zip','-C',dir],{windowsHide:true});
}
await download('https://cdn.jsdelivr.net/gh/nlohmann/json@v3.12.0/single_include/nlohmann/json.hpp',dir+'/json.hpp');
if(!fs.existsSync(dir+'/w64devkit/bin/g++.exe')){
 await download('https://github.com/skeeto/w64devkit/releases/download/v2.10.0/w64devkit-x64-2.10.0.7z.exe',dir+'/compiler.7z.exe','18d0a4c71a166f8401ab6305781bec5882b40b5e06ba9807c61cb5f3b3c6325e');
 await run(dir+'/compiler.7z.exe',['-y','-o./native/vendor'],{windowsHide:true});
}
console.log('Native build dependencies are ready.');
