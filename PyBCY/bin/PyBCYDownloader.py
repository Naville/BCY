#!/usr/bin/env python
# -*- coding: utf-8 -*-
from PyBCY import *
import argparse
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--Email",help="LoginEmail", required=True)
    parser.add_argument("--Password",help="LoginPassword",required=True)
    parser.add_argument("--SavePath",help="SavePath",required=True)
    args = parser.parse_args()
    BCYDownloadUtils.BCYDownloadUtils(args.Email, args.Password,args.SavePath,MaxDownloadThread=16,MaxQueryThread=32,Daemon=True)
    while True:
        pass
