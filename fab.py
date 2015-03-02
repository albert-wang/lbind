#!/usr/bin/python
from fabricate import *
import glob
import os

CFLAGS = "-O2 -Wall -Wno-unknown-pragmas -Wno-c++11-extensions -Wno-unused-variable -g -std=c++11 -stdlib=libc++ -Iinclude -I../lua/src".split()
LDFLAGS = "-lboost_thread -lboost_system -lc++ -llua -L.".split()

def out(d, fname) :
	name = os.path.splitext(os.path.basename(fname))[0]
	return os.path.join(d, name + ".o")

def build() :
	compile('src', 'obj')
	link('obj', 'lbind')

def testing() :
	CFLAGS.append("-DUNIT_TESTING")
	CFLAGS.append("-DBOOST_TEST_DYN_LINK")
	CFLAGS.append("-I./test")

	LDFLAGS.append("-lboost_unit_test_framework")

	compile('test', 'tobj')
	compile('src', 'tobj')
	link('tobj', 'unittest')

def compile(dir, output, flags=None) :
	sources = glob.glob(dir + "/*.cpp")
	for source in sources :
		run("clang++", "-c", source, "-o", out(output, source), CFLAGS, flags)

def link(dir, output, flags=None) :
	objects = glob.glob(dir + "/*.o")
	run("clang++", "-o", output, objects, LDFLAGS, flags)

main(parallel_ok=True, jobs=8)
