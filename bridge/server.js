const net = require('net');
const childProcess = require('child_process');
const path = require('path');

const PORT = parseInt(GetConvar('fivem-cpp-sdk_port', '30698'), 10);
const TICK_MS = parseInt(GetConvar('fivem-cpp-sdk_tick_ms', '50'), 10);

/** @type {Map<number, {socket, name, buf}>} */
const clients = new Map();
let nextId = 1;

/** @type {Map<string, import('child_process').ChildProcess>} */
const processes = new Map();

/** @type {Set<string>} */
const hookedEvents = new Set();

function frame(obj) {
    const payload = Buffer.from(JSON.stringify(obj), 'utf8');
    const hdr = Buffer.allocUnsafe(4);
    hdr.writeUInt32BE(payload.length, 0);
    return Buffer.concat([hdr, payload]);
}

function sendTo(id, obj) {
    const c = clients.get(id);
    if (c) try { c.socket.write(frame(obj)); } catch (_) {}
}

function broadcast(obj) {
    const buf = frame(obj);
    for (const [, c] of clients)
        try { c.socket.write(buf); } catch (_) {}
}

function spawnResource(resourceName) {
    if (processes.has(resourceName)) return;

    const binary = GetResourceMetadata(resourceName, 'binary', 0);
    if (!binary) return;

    const resourcePath = GetResourcePath(resourceName);
    const binaryPath = path.join(resourcePath, binary);

    console.log(`[fivem-cpp-sdk] Spawning '${resourceName}' → ${binaryPath}`);

    const child = childProcess.spawn(binaryPath, [], {
        env: { ...process.env, FXCPP_PORT: String(PORT), FXCPP_RESOURCE: resourceName },
        stdio: ['ignore', 'pipe', 'pipe'],
    });

    child.stdout.on('data', d => process.stdout.write(`[${resourceName}] ${d}`));
    child.stderr.on('data', d => process.stderr.write(`[${resourceName}] ${d}`));

    child.on('error', err => {
        console.error(`[fivem-cpp-sdk] Failed to start '${resourceName}': ${err.message}`);
        processes.delete(resourceName);
    });

    child.on('exit', (code, signal) => {
        console.log(`[fivem-cpp-sdk] '${resourceName}' exited (code=${code} signal=${signal})`);
        processes.delete(resourceName);
    });

    processes.set(resourceName, child);
}

function killResource(resourceName) {
    const child = processes.get(resourceName);
    if (!child) return;
    console.log(`[fivem-cpp-sdk] Stopping '${resourceName}'`);
    child.kill('SIGTERM');
    processes.delete(resourceName);
}

on('onResourceStart', (resourceName) => {
    if (resourceName === GetCurrentResourceName()) return;
    spawnResource(resourceName);
});

on('onResourceStop', (resourceName) => {
    killResource(resourceName);
    for (const [id, c] of clients) {
        if (c.name === resourceName) {
            try { c.socket.destroy(); } catch (_) {}
            clients.delete(id);
        }
    }
});

setImmediate(() => {
    const numResources = GetNumResources();
    for (let i = 0; i < numResources; i++) {
        const name = GetResourceByFindIndex(i);
        if (name && name !== GetCurrentResourceName()) {
            spawnResource(name);
        }
    }
});

function hookEvent(eventName) {
    if (hookedEvents.has(eventName)) return;
    hookedEvents.add(eventName);

    on(eventName, (...args) => {
        const src = String(typeof source !== 'undefined' ? source : -1);
        const safeArgs = args.map(a => (a === undefined ? null : a));
        broadcast({ t: 'event', event: eventName, args: safeArgs, source: src });
    });
}

function dispatch(id, msg) {
    const c = clients.get(id);
    if (!c) return;

    switch (msg.t) {

        case 'hello':
            c.name = msg.resource ?? `cpp-${id}`;
            sendTo(id, { t: 'ready' });
            console.log(`[fivem-cpp-sdk] '${c.name}' connected`);
            break;

        case 'sub':
            if (typeof msg.event === 'string') hookEvent(msg.event);
            break;

        case 'registerCommand': {
            const cmdName = msg.command;
            if (typeof cmdName !== 'string') break;
            RegisterCommand(cmdName, (src, args) => {
                sendTo(id, { t: 'cmd', command: cmdName, source: String(src), args });
            }, false);
            break;
        }

        case 'emit':
            if (typeof msg.event === 'string')
                emit(msg.event, ...(Array.isArray(msg.args) ? msg.args : []));
            break;

        case 'emitNet': {
            const target = msg.target ?? -1;
            if (typeof msg.event === 'string')
                emitNet(msg.event, target, ...(Array.isArray(msg.args) ? msg.args : []));
            break;
        }

        case 'native': {
            const reqId = msg.id;
            try {
                const hash = BigInt(msg.hash ?? 0);
                const result = Citizen.invokeNativeByHash(0, hash, ...(Array.isArray(msg.args) ? msg.args : []));
                sendTo(id, { t: 'nr', id: reqId, r: result ?? null });
            } catch (err) {
                sendTo(id, { t: 'nr', id: reqId, r: null, err: String(err) });
            }
            break;
        }

        case 'trace':
            if (typeof msg.msg === 'string')
                console.log(`[${c.name}] ${msg.msg}`);
            break;

        default:
            console.warn(`[fivem-cpp-sdk] Unknown message '${msg.t}' from '${c.name}'`);
    }
}

setInterval(() => {
    if (clients.size > 0) broadcast({ t: 'tick' });
}, TICK_MS);

const server = net.createServer((socket) => {
    const id = nextId++;
    clients.set(id, { socket, name: `cpp-${id}`, buf: Buffer.alloc(0) });

    socket.on('data', (chunk) => {
        const c = clients.get(id);
        if (!c) return;
        c.buf = Buffer.concat([c.buf, chunk]);

        while (c.buf.length >= 4) {
            const len = c.buf.readUInt32BE(0);
            if (c.buf.length < 4 + len) break;
            const raw = c.buf.slice(4, 4 + len).toString('utf8');
            c.buf = c.buf.slice(4 + len);
            try { dispatch(id, JSON.parse(raw)); }
            catch (err) { console.error(`[fivem-cpp-sdk] Bad message from ${id}: ${err}`); }
        }
    });

    socket.on('close', () => {
        const name = clients.get(id)?.name ?? `cpp-${id}`;
        console.log(`[fivem-cpp-sdk] '${name}' disconnected`);
        clients.delete(id);
    });

    socket.on('error', (err) => {
        console.error(`[fivem-cpp-sdk] Socket error (${id}): ${err.message}`);
    });
});

server.listen(PORT, '127.0.0.1', () => {
    console.log(`[fivem-cpp-sdk] Bridge listening on 127.0.0.1:${PORT}`);
});

server.on('error', err => console.error(`[fivem-cpp-sdk] Server error: ${err.message}`));
