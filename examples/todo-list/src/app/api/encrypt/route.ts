import { NextResponse } from 'next/server';
import crypto from 'crypto';
import { Verokv } from 'verokv-ts';

const verokv = new Verokv('localhost', 6381);

// Function to generate a random secret with "vero_" prefix
function generateSecret() {
  return `vero_${crypto.randomBytes(16).toString('hex')}`;
}

// Function to hash a secret
function hashSecret(secret: string) {
  return crypto.createHash('sha256').update(secret).digest('hex');
}

// Function to encrypt a message
function encryptMessage(secret: string, message: string) {
  const key = crypto.createHash('sha256').update(secret).digest();
  const cipher = crypto.createCipheriv('aes-256-ctr', key, Buffer.alloc(16, 0));
  return cipher.update(message, 'utf8', 'hex') + cipher.final('hex');
}

// Function to decrypt a message
function decryptMessage(secret: string, encryptedMessage: string) {
  const key = crypto.createHash('sha256').update(secret).digest();
  const iv = Buffer.alloc(16, 0);
  const decipher = crypto.createDecipheriv('aes-256-ctr', key, iv);
  return decipher.update(encryptedMessage, 'hex', 'utf8') + decipher.final('utf8');
}

async function getWithTimeout(key: string, timeout: number) {
  const getPromise = verokv.get(key); // This returns a Promise from Verokv
  const timeoutPromise = new Promise<null>((_, reject) => 
    setTimeout(() => reject(new Error('Timeout: No Secret found')), timeout)
  );
  
  // Race the two promises (get from Verokv vs timeout)
  return Promise.race([getPromise, timeoutPromise]);
}

export async function POST(request: Request) {
  try {
    const { operation, message, secret } = await request.json();

    if (!operation) {
      return NextResponse.json({ error: 'Operation type is required' }, { status: 400 });
    }

    if (operation === 'encrypt' && message) {
      // Perform encryption
      const secret = generateSecret(); // Generate a new secret
      const secretHash = hashSecret(secret); // Hash the secret
      const encryptedMessage = encryptMessage(secret, message); // Encrypt the message

      // Store the hashed secret and encrypted message in the database
      await verokv.set(secretHash, encryptedMessage); // Save encrypted message to Verokv

      return NextResponse.json({ secret });
    } 

    if (operation === 'decrypt' && secret) {
      const secretHash = hashSecret(secret); // Hash the provided secret

      let encryptedMessage: string | null = null;
      try {
        // Retrieve the encrypted message using the secret hash
        encryptedMessage = await getWithTimeout(secretHash, 5000); // Get with timeout (racing get vs timeout)
      } catch (error) {
        console.error(error);
        return NextResponse.json({ error: 'No Secret found' }, { status: 404 });
      }

      if (encryptedMessage) {
        // Decrypt the retrieved encrypted message
        const decryptedMessage = decryptMessage(secret, encryptedMessage);
        return NextResponse.json({ message: decryptedMessage });
      } else {
        return NextResponse.json({ error: 'Encrypted message is missing' }, { status: 404 });
      }
    }

    return NextResponse.json({ error: 'Invalid operation or missing parameters' }, { status: 400 });
  } catch (error: unknown) {
    console.error('Route Error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
