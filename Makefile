TESTS = test/*.js

all: build test

build: clean configure compile

configure:
	node-gyp configure

compile: configure
	node-gyp build

clean:
	node-gyp clean


.PHONY: clean test build
