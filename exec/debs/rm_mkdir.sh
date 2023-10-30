#!/bin/bash
rm -rf $1 || true
rm 1.zip || true
mkdir -p $1/opt/rbths/bin/
mkdir -p $1/opt/rbths/plugins/
mkdir -p $1/usr/lib/systemd/system/
mkdir -p $1/DEBIAN
