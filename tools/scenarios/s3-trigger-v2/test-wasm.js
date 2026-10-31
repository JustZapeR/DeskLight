const fs = require('fs');
const assert = require('assert');
const bytes = fs.readFileSync(__dirname + '/trigger.chip.wasm');
const moduleWasm = new WebAssembly.Module(bytes);
const imports = WebAssembly.Module.imports(moduleWasm);
const exportsWasm = WebAssembly.Module.exports(moduleWasm);
assert(imports.some(x => x.name === 'memory' && x.kind === 'memory'));
assert(exportsWasm.some(x => x.name === '__indirect_function_table' && x.kind === 'table'));
const memory = new WebAssembly.Memory({initial: 16});
let instance, level = 0, out = 0, writes = [], pending, watch, timer;
const u32 = p => new DataView(memory.buffer).getUint32(p, true);
const string = p => { let x = new Uint8Array(memory.buffer); let s = ''; while (x[p]) s += String.fromCharCode(x[p++]); return s; };
const env = {memory,
 pinInit(p, mode) { const name = string(p); if (name === 'MARK') { assert.equal(mode, 0); return 1; } assert.equal(name, 'PIR'); assert.equal(mode, 16); return 2; },
 timerInit(p) { timer = {user:u32(p), cb:u32(p+4)}; return 3; },
 pinWatch(pin,p) { assert.equal(pin,1); watch={user:u32(p), edge:u32(p+4), cb:u32(p+8)}; assert.equal(watch.edge,1); return 1; },
 pinRead(pin) { assert.equal(pin,1); return level; },
 pinWrite(pin,v) { assert.equal(pin,2); out=v; writes.push(v); },
 timerStart(id,us,repeat) { assert.equal(id,3); assert.equal(repeat,0); pending=us; }
};
instance = new WebAssembly.Instance(moduleWasm,{env}); instance.exports.chipInit();
const table = instance.exports.__indirect_function_table;
const rise = () => table.get(watch.cb)(watch.user,1,1);
const tick = () => table.get(timer.cb)(timer.user);
level=1; rise(); assert.equal(pending,100); level=0; tick(); assert.deepEqual(writes,[]);
level=1; rise(); assert.equal(pending,100); tick(); assert.equal(out,1); assert.equal(pending,60000);
rise(); assert.deepEqual(writes,[1]); tick(); assert.equal(out,0); assert.equal(pending,100000);
rise(); assert.deepEqual(writes,[1,0]); tick(); rise(); tick(); assert.deepEqual(writes,[1,0,1]);
console.log('PASS: actual WASM memory/table ABI, 100us gate, 60ms high, 100ms refractory, short pulse rejected');
