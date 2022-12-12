#!/bin/sh

gcc server.c -o server -g -lpthread

chmod 744 server 
