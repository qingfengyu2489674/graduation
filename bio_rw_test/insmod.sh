#!/bin/bash

sudo insmod bio_rw_char_dev.ko

sudo chmod 666 /dev/bio_rw_char_dev
