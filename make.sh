#!/bin/bash

CFLAGS="-g -I/home/chalos/openh264/openh264/codec/api/svc/ -I. -I../"
LDFLAGS="-lccrtp -lucommon -lcommoncpp -lopenh264"

g++ EncoderDecoder.cpp Encoder.cpp Decoder.cpp $CFLAGS $LDFLAGS -o EncoderDecoder
