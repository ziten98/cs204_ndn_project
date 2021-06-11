#!/usr/bin/python3
from mininet.net import Mininet
from mininet.cli import CLI
from mininet.link import TCIntf, Intf, TCLink
from mininet.node import Node
import json
import argparse
import os

class LinuxRouter(Node):
    "A node with IP forwarding enabled"

    def config(self, **params):
        super(LinuxRouter, self).config(**params)
        # Enable forwarding on the router
        self.cmd('sysctl net.ipv4.ip_forward=1')

    def terminate(self):
        self.cmd('sysctl net.ipv4.ip_forward=0')
        super(LinuxRouter, self).terminate()


def link_helper(net, routers, i, j):
    '''This is a helper function to help connect between routers. '''

    a = min(i, j)
    b = max(i, j)
    Ra = routers['R%d' % a]
    Rb = routers['R%d' % b]
    net.addLink(Ra, Rb, cls=TCLink, delay='20ms', intfName1='R%d-%d%d' % (a, a, b), params1={'ip': '10.0.%d%d.%d/24' % (a, b, a)},
                intfName2='R%d-%d%d' % (b, a, b), params2={'ip': '10.0.%d%d.%d/24' % (a, b, b)})
    Ra.ndn_neighbors.append({'ip': '10.0.%d%d.%d' % (a, b, b), 'name': 'R%d' % b})
    Rb.ndn_neighbors.append({'ip': '10.0.%d%d.%d' % (a, b, a), 'name': 'R%d' % a})


def build_topo():
    net = Mininet()
    routers = {}
    switches = {}
    servers = {}
    clients = {}

    for i in range(1, 5):
        # add routers
        Rname = 'R%d' % i
        routers[Rname] = net.addHost(Rname, cls=LinuxRouter, ip='10.0.%d.1/24' % i)
        routers[Rname].ndn_neighbors = []

        # add switches
        SWname = 'SW%d' % i
        switches[SWname] = net.addSwitch(SWname)

        # add servers
        Sname = 'S%d' % i
        servers[Sname] = net.addHost(Sname, ip='10.0.%d.2/24' % i)
        servers[Sname].ndn_neighbors = []

        # add clients
        Cname = 'C%d' % i
        clients[Cname] = net.addHost(Cname, ip='10.0.%d.3/24' % i)
        clients[Cname].ndn_neighbors = []

        # set up the links between routers and hosts and servers to form a LAN
        net.addLink(Rname, SWname, cls=TCLink, delay='2ms')
        net.addLink(SWname, Sname, cls=TCLink, delay='2ms')
        net.addLink(SWname, Cname, cls=TCLink, delay='2ms')

        clients[Cname].ndn_neighbors.append({'ip': '10.0.%d.1' % i, 'name': Rname})
        servers[Sname].ndn_neighbors.append({'ip': '10.0.%d.1' % i, 'name': Rname})

        routers[Rname].ndn_neighbors.append({'ip': '10.0.%d.2' % i, 'name': Sname})
        routers[Rname].ndn_neighbors.append({'ip': '10.0.%d.3' % i, 'name': Cname})


    con0 = net.addController('con0')

    # set up the links between routers
    link_helper(net, routers, 1, 2)
    link_helper(net, routers, 2, 3)
    link_helper(net, routers, 3, 4)
    
    # startup switches
    for key, val in switches.items():
        val.start([con0])


    net.start()
    net.waitConnected()
    net.pingAll()

    # set up the environment on the nodes
    for key, val in routers.items():
        val.cmd('cd test/%s' % key)
        val.cmd("echo '%s' > topo.json" % json.dumps({"name": key, "neighbors": val.ndn_neighbors}, indent=4))
        val.cmd('redis-server > redis.log 2>&1 &')
        val.cmd('../../build/router > router.log 2>&1 &')
    
    for key, val in servers.items():
        val.cmd('cd test/%s' % key)
        val.cmd("echo '%s' > topo.json" % json.dumps({"name": key, "neighbors": val.ndn_neighbors}, indent=4))
        val.cmd('../../build/server > server.log 2>&1 &')
    
    for key, val in clients.items():
        val.cmd('cd test/%s' % key)
        val.cmd("echo '%s' > topo.json" % json.dumps({"name": key, "neighbors": val.ndn_neighbors}, indent=4))
    
    CLI( net )
    net.stop()

    os.system('sudo pkill -u 0 -f redis-server')


if __name__ == '__main__':
    print('setting up topology')
    build_topo()
        
