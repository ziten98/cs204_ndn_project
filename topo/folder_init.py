#!/usr/bin/python3
import json
import argparse
import os

# initialize certain numbers of folders and files for testing

def init_folders_and_files(num):
    os.system('mkdir test')
    for i in range(1, num+1):
        os.system('topo/router_init.sh R%d' % i)
        os.system('topo/client_init.sh C%d' %i)
        os.system('topo/server_init.sh S%d' %i)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('num', help='the number of folders')
    args = parser.parse_args()

    init_folders_and_files(int(args.num))
