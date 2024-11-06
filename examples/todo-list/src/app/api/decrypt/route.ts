import { NextResponse } from 'next/server';
import crypto from 'crypto';
import { Verokv } from 'verokv-ts';

const verokv = new Verokv('localhost', 6381);

// Function to hash a secret
function hashSecret(secret: string) {
  return crypto.createHash('sha256').update(secret).digest('hex');
}


function decryptMessage(secret: string, encryptedMessage: string) {
  const key = crypto.createHash('sha256').update(secret).digest();
  const iv = Buffer.alloc(16, 0);
  const decipher = crypto.createDecipheriv('aes-256-ctr', key, iv);
  return decipher.update(encryptedMessage, 'hex', 'utf8') + decipher.final('utf8');
}

async function getWithTimeout(key: string, timeout: number) {
  const getPromise = verokv.get(key);
  const timeoutPromise = new Promise<null>((_, reject) => 
    setTimeout(() => reject(new Error('Timeout: No Secret found')), timeout)
  );
  
  return Promise.race([getPromise, timeoutPromise]);
}

export async function POST(request: Request) {
  try {
    const { secret } = await request.json();

    const secretHash = hashSecret(secret);

    // Check if the secret exists in the database
    let encryptedMessage: string | null = null;
    try {
      encryptedMessage = await getWithTimeout(secretHash, 5000);
    } catch (error) {
      console.error(error)
      return NextResponse.json({ error: 'No Secret found' }, { status: 404 });
    }

    const decryptedMessage = decryptMessage(secret, encryptedMessage!);

    return NextResponse.json({ message: decryptedMessage });
  } catch (error: unknown) {
    console.error('Decryption Route Error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
