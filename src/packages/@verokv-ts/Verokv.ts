import * as net from 'net';

class Verokv {
    private client: net.Socket;

    constructor(host: string, port: number) {
        this.client = new net.Socket();
        this.client.connect(port, host, () => {
            console.log('Connected to server');
        });

        this.client.on('error', (err: Error) => {
            console.error('Connection error:', err.message);
        });
    }

    public set(key: string, value: string): Promise<void> {
        return new Promise((resolve, reject) => {
            const command = `set ${key} ${value}\n`;
            this.client.write(command);

            this.client.once('data', (data: Buffer) => {
                const response = data.toString().trim();
                const cleanResponse = response.startsWith('$') ? response.slice(1) : response;
                console.log('Set response:', cleanResponse);
                resolve();
            });

            this.client.once('error', (err: Error) => {
                reject(err);
            });
        });
    }

    public get(key: string): Promise<string> {
        return new Promise((resolve, reject) => {
            const command = `get ${key}\n`;
            this.client.write(command);

            this.client.once('data', (data: Buffer) => {
                const response = data.toString().trim();
                const cleanResponse = response.startsWith('$') ? response.slice(1) : response;
                resolve(cleanResponse);
            });

            this.client.once('error', (err: Error) => {
                reject(err);
            });
        });
    }
}

export default Verokv;
