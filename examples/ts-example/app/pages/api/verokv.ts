import { NextApiRequest, NextApiResponse } from 'next';
import { Verokv } from 'verokv-ts';

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  try {
    const verokv = new Verokv('localhost', 6379);
    await verokv.set('username', 'johndoe');
    const value = await verokv.get('username');
    console.log('API route hit: Value retrieved:', value); 
    res.status(200).json({ username: value });
  } catch (err: any) {
    console.error('API route error:', err); 
    res.status(500).json({ error: err.message });
  }
}
