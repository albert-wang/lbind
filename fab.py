from fabricate import *
import glob
import os

CFLAGS = "-Wall -Wno-unknown-pragmas -Wno-c++11-extensions -Wno-unused-variable -g -std=c++11 -stdlib=libc++ -Iinclude -I../lua/src".split()
LDFLAGS = "-lboost_thread -lboost_system -lc++ -llua -L.".split()

def out(fname) :
	name = os.path.splitext(os.path.basename(fname))[0]
	return os.path.join("obj", name + ".o")

def build() :
	compile()
	link()

def compile(flags=None) :
	sources = glob.glob("src/*.cpp")
	for source in sources :
		run("clang++", "-c", source, "-o", out(source), CFLAGS, flags)

def link(flags=None) :
	objects = glob.glob("obj/*.o")
	run("clang++", "-o", "testing", objects, LDFLAGS, flags)

main(parallel_ok=True, jobs=8)
