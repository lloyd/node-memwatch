TESTS = test/*.js

all: build

build: clean configure compile

configure:
	node-waf configure

compile:
	node-waf build

clean:
	#rm -f bcrypt_lib.node
	rm -Rf build


.PHONY: clean test build
