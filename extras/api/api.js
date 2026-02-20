export class DOSBoxApi {
    constructor(baseUrl = '/api') {
        this.baseUrl = baseUrl.replace(/\/$/, '');
    }

    async _request(path, options = {}) {
        const response = await fetch(`${this.baseUrl}/${path}`, options);
        if (!response.ok && response.status !== 412) {
            throw new Error(`DOSBox API Error: ${response.status} ${response.statusText}`);
        }
        return response;
    }

    async getCpu() {
        const res = await this._request('/cpu');
        return await res.json();
    }

    async readMem(...args) {
        if (args.length === 2) {
            args.unshift(0);
        }
        const [segment, offset, len] = args;
        const seg = segment !== null ? `${segment}/` : '';
        const url = `memory/${seg}${offset}/${len}`;
        const res = await this._request(url, { headers: { 'Accept': 'application/octet-stream' } });
        return new Uint8Array(await res.arrayBuffer());
    }

    async readMemAndCpu(...args) {
        if (args.length === 2) {
            args.unshift(0);
        }
        const [segment, offset, len] = args;
        const seg = segment !== null ? `${segment}/` : '';
        const url = `memory/${seg}${offset}/${len}`;
        const res = await this._request(url, {
            headers: { 'Accept': 'application/json' }
        });
        const j = await res.json();
        const data = atob(j.memory.data);
        const mem = new Uint8Array(data.length);
        for (let i = 0; i < data.length; i++) {
            mem[i] = data.charCodeAt(i);
        }
        j.memory.data = mem;
        return j;
    }

    async writeMem(...args) {
        if (args.length === 2) {
            args.unshift(0);
        }
        const [segment, offset, data] = args;
        const seg = segment !== null ? `${segment}/` : '';
        await this._request(`memory/${seg}${offset}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/octet-stream' },
            body: data
        });
    }

    async compareAndSwap(...args) {
        if (args.length === 3) {
            args.unshift(0);
        }
        const [segment, offset, data, expected] = args;
        const seg = segment !== null ? `${segment}/` : '';
        let expectedEncoded;
        if (typeof Buffer !== "undefined" && expected instanceof Buffer) {
            expectedEncoded = expected.toString('base64');
        } else {
            expectedEncoded = btoa(String.fromCharCode(...expected));
        }
        const res = await this._request(`memory/${seg}${offset}`, {
            method: 'PUT',
            headers: {
                'Content-Type': 'application/octet-stream',
                'If-Match': expectedEncoded
            },
            body: data
        });
        const j = await res.json();
        if (res.status === 412) {
            const actualData = atob(j.memory.data);
            const mem = new Uint8Array(data.length);
            for (let i = 0; i < actualData.length; i++) {
                mem[i] = actualData.charCodeAt(i);
            }
            return mem;
        }
        return null;
    }

    async alloc(sizeBytes, area = 'conv', strategy = 'best_fit') {
        const res = await this._request('memory/allocate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ size: sizeBytes, area: area,  strategy: strategy })
        });
        return await res.json();
    }

    async free(physicalAddress) {
        await this._request('memory/free', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ addr: physicalAddress })
        });
    }

    async getDosInfo() {
        const res = await this._request('dos');
        return await res.json();
    }
}
