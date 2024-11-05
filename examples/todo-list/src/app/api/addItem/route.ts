import {  NextResponse } from 'next/server';
import { verokv } from '@/app/lib/verokv';

export async function GET() {
  try {
    
    await verokv.set('myKey', 'user_id');

    const value = await verokv.get('myKey');

    if (value === null) {
      return NextResponse.json({ error: 'Username not found' }, { status: 404 });
    }
    console.log("value", value)
    return NextResponse.json({ username: value });
  } catch (error: unknown) {
    console.error('API route error:', error);
    return NextResponse.json({ error: 'Internal Server Error: ' }, { status: 500 });
  }
}
