# cs204_ndn_project

## General Information
This is our final project for CS 204: Advanced Computer Networks.

We learn the concepts of Named Data Neworking and implement an NDN architecture prototype using Mininet.

The sole reliability of IP in the internet model has long shown disadvantages and alternative solutions have been in research for quite some time. One such interesting area of research is the Named Data Networking (NDN) model that uses more understandable forms of notations to identify and mark content over the internet. One important parameter to evaluate efficiency in such models is load balancing, the way data is transmitted over the network. That along with NDN being a human readable model, the challenges and the potential it holds in replacing the DNS model are the main reasons for us to research in this area. In order to achieve better performance, we propose a load balancing model based on several metrics, including the number of hops to reach the destination, the delay to reach the destination and the pending interest queue size. We built the NDN architecture from scratch, implemented all the basic infrastructure of a NDN network and based on that, implemented an efficient load balancing forwarding strategy. This is not a trivial implementation because 2779 lines of code in total is written, including 76.4% C++ code and 19.8% Python code. All the code is available on Github [6]. The evaluation results show significant improvement in the performance and utilization of the network than the existing models, which only forward interest packets to the path that has a minimum number of hops.

## Group Members
Ziteng Zeng, Arun Venkatesh, Ponmanikandan Velmurugan


## How to test

1. clone the git repository
2. run init.sh
3. run "sudo topo/topo1.py"
4. in the mininet, you can type "R1 ../../stat.py" to see statistics on router R1, or you can type "C1 ../../request.py 'S4' 1k.txt --save 1k.txt" to make a request on /S4/1k.txt

If you want to enable load balance, build with macro NDN_LOAD_BALANCE defined.

If you want to enable cache, build with macro NDN_USE_CACHE defined.