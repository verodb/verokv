import { useEffect } from 'react';
import { Verokv } from 'verokv-ts';

const VerokvComponent = () => {
  useEffect(() => {
    const verokv = new Verokv('localhost', 6379);

    verokv.set('username', 'johndoe')
      .then(() => verokv.get('username'))
      .then((value) => {
        console.log('Retrieved username:', value);
      })
      .catch((err) => {
        console.error('Error:', err);
      });
  }, []);

  return <div>Check your console for the VeroKV output.</div>;
};

export default VerokvComponent;
