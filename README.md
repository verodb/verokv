# VeroKV / oss

VeroKV is a key-value based in-memory database created by VeroDB. We support all the redis commands on this databse. Although we allow users to self host the project. It is meant for research purposes only and not production. 


## Build Instructions
Instructions to build the project from source.
```bash
# clone this repository
git clone https://github.com/verodb/verokv && cd verokv 

# cd into the repository
cd verokv

# build the server (verokv)
make

# build the the client (verokv-cli)
make client

# run the unit tests
make test
```

## Usage
`verokv` is the server's executable, while `verokv-cli` is the client's. You
can run them separately or use the utility script `connect.sh` to run both at
once (useful when developing).
```bash
# run the utility script
chmod +x connect.sh && ./connect.sh

# run the server
./verokv

# run the client
./verokv-cli
```

## Commands supported
```
cmds list:
---
str cmds:        hash cmds:       list cmds:      set cmds:  
- [x] set        - [x] hset       - [x] lpush     - [x] sadd 
- [x] get        - [x] hget       - [x] rpush     - [x] srem 
- [x] mset       - [x] hdel       - [x] lpop      - [x] smembers  
- [x] mget       - [x] hgetall    - [x] rpop      - [x] sismember  
- [x] incr       - [x] hexists    - [x] llen      - [ ] scard  
- [x] decr       - [x] hkeys      - [x] lindex    - [x] smismember
- [x] incrby     - [x] hvals      - [x] lpos      - [ ] sdiff
- [x] decrby     - [x] hlen       - [x] lset      - [ ] sinter
- [x] strlen     - [ ] hincrby    - [x] lrem      - [ ] sunion
- [ ] append     - [x] hmget      - [x] lrange    - [ ] sdiffstore
- [ ] setrange   - [ ] hstrlen    - [ ] lpushx    - [ ] sinterstore
- [ ] getrange   - [ ] hsetnx     - [ ] rpushx    - [ ] sunionstore
- [ ] setnx      - [ ]            - [ ] ltrim     - [ ]
- [ ] msetnx                      - [ ]           
- [ ] 


keys cmds:      etc:
- [x] del       - [ ] ping
- [x] exists    - [x] quit
- [x] type      - [x] shutdown
- [ ] rename    - [ ] 
- [ ]
```

## License
Distributed under the MIT License. See [LICENSE](/LICENSE) for more information.

