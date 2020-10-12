## Welcome to the repository GOLOS blockchain

- Free transactions
- Fast block confirmations (3 seconds)
- Hierarchical role based permissions (keys)
- Delegated Proof-of-Stake Consensus (DPOS)
- Good choice for placing your own dApps

## Documentation

Instructions for launching a [delegate node](https://wiki.golos.id/witnesses/node/guide), a public [API node](https://wiki.golos.id/witnesses/node/guide-api), and launching a blockchain [node for exchanges](https://wiki.golos.id/witnesses/node/guide-exchange) are available in the community Wiki.

## Quickstart

Just want to get up and running quickly?  Try deploying a prebuilt dockerized container, image is available at docker hub - https://hub.docker.com/r/golosblockchain/golos/tags

Docker image tags:

- **latest** - built from master branch, used to run mainnet Golos
- **testnet** - built from master branch, could be used to run test network

```
sudo docker run -d \
    -p 4243:4243 \
    -p 8090:8090 \
    -p 8091:8091 \
    -v /etc/golosd:/etc/golosd \
    -v /var/lib/golosd:/var/lib/golosd \
    --name golosd golosblockchain/golos:latest

sudo docker logs -f golosd
```    

To attach to the golosd you should use the cli_wallet:
```
sudo docker exec -ti golosd \
    /usr/local/bin/cli_wallet \
    --server-rpc-endpoint="ws://127.0.0.1:8091"
```

## Seed Nodes

A list of some seed nodes to get you started can be found in
[share/golosd/seednodes](share/golosd/seednodes). This same file is baked into the docker images.
