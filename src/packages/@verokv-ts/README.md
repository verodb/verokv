# Verokv SDK

`Verokv` is a lightweight Node.js client SDK for interacting with a key-value store server over a TCP connection. It allows you to easily connect to the server and perform `set` and `get` operations on key-value pairs.

### Pre-requisites:

Make sure you have Verokv server setup. [Read more here](https://github.com/verodb/verokv?tab=readme-ov-file)

## Installation

To use Verokv in your project, add it to your project dependencies.

```
npm install verokv-ts

# OR 

pnpm add verokv-ts

# OR 

yarn install verokv-ts
```

## Usage

### 1. Import the SDK

```javascript
import Verokv from 'verokv';
```

### 2. Create an instance 

Initialize a Verokv instance by providing the server's hostname and port number.

```javascript
const verokv = new Verokv('localhost', 6379);
```

### 3. Set a Key-Value Pair

Use the set method to store a value with a specified key.

```javascript
verokv.set('name', 'Alice')
    .then(() => {
        console.log('Key-value pair set successfully');
    })
    .catch((err) => {
        console.error('Error setting key-value:', err);
    });
```

### 4. Retrieve a Value by Key

Use the get method to retrieve the value of a specific key.

```javascript
verokv.get('name')
    .then((value) => {
        console.log('Retrieved value:', value);
    })
    .catch((err) => {
        console.error('Error retrieving key-value:', err);
    });
```

### Error Handling

Verokv handles connection and request errors automatically. If an error occurs during a set or get operation, the returned promise will be rejected with an error message.

### Example

Here's a complete example of using Verokv to connect to a key-value store server, set a key-value pair, and retrieve it.

```javascript
import Verokv from 'verokv';

const verokv = new Verokv('localhost', 6379);

verokv.set('username', 'johndoe')
    .then(() => {
        return verokv.get('username');
    })
    .then((value) => {
        console.log('Retrieved username:', value);
    })
    .catch((err) => {
        console.error('Operation error:', err);
    });
```




