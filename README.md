# Golos - the blockchain for decentralized applications

[![Build Status](https://travis-ci.org/golos-blockchain/golos.svg?branch=master)](https://travis-ci.org/golos-blockchain/golos)

Welcome to the official repository GOLOS blockchain.

- Free transactions
- Fast block confirmations (3 seconds)
- Hierarchical role based permissions (keys)
- Delegated Proof-of-Stake Consensus (DPOS)
- Good choice for placing your own dApps

# Code is Documentation

Rather than attempt to describe the rules of the blockchain, it is up to
each individual to inspect the code to understand the consensus rules.

# Quickstart

Just want to get up and running quickly?  Try deploying a prebuilt dockerized container. 

```
sudo docker run -d \
    -p 4243:4243 \
    -p 8090:8090 \
    -p 8091:8091 \
    --name golos-default golosblockchain/golos:latest
```    

To attach to the golosd you should use the cli_wallet:
```
sudo docker exec -ti golos-default \
    /usr/local/bin/cli_wallet \
    --server-rpc-endpoint="ws://127.0.0.1:8091"
```

# Building

See the [hint](https://github.com/golos-blockchain/golos/wiki) or outdated [build instruction](https://github.com/GolosChain/golos/wiki/Build-instruction), which contains information about configuring, building and running of docker containers.

# Testing

```
git clone https://github.com/golos-blockchain/golos.git
cd golos
sudo docker rm local/golos-test
sudo docker build -t local/golos-test -f share/golosd/docker/Dockerfile-test .
```

# Seed Nodes

A list of some seed nodes to get you started can be found in
[share/golosd/seednodes](share/golosd/seednodes).
This same file is baked into the docker images.

# No Support & No Warranty

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
