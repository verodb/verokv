import { Verokv } from 'verokv-ts';

const host = '127.0.0.1';
const port = 6381;

const verokv = new Verokv(host, port);

async function run() {
    try {
        await verokv.set('myKey', 'myValue');

        const value = await verokv.get('myKey');
        console.log(`Value retrieved for 'myKey': ${value}`);
    } catch (error) {
        console.error('Error:', error);
    }
}

run();
