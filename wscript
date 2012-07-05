# -*- mode: python -*-

import Options, Utils, sys, os

srcdir = "."
blddir = "build"
VERSION = "0.0.1"
node_version = os.popen("node --version").read()

def set_options(opt):
  opt.tool_options("compiler_cxx")
  opt.tool_options("compiler_cc")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("compiler_cc")
  conf.check_tool("node_addon")
  o = Options.options

  libpath = ['/lib', '/usr/lib', '/usr/local/lib', '/opt/local/lib', '/usr/sfw/lib']
  includes = ['/usr/include', '/usr/includes', '/usr/local/includes', '/opt/local/includes', '/usr/sfw/lib'];

def build(bld):
  gcstats = bld.new_task_gen("cxx", "shlib", "node_addon")
  gcstats.cxxflags =  [ "-O3" ]
  if node_version.startswith("v0.8"):
    # the v8 in node 0.8.x doesn't ever send a kGCCallbackFlagCompacted -
    # instead we key of GC type.  I don't fully understand this behavioral
    # change, but the numbers we get are still stable / similar to 0.6.
    gcstats.cxxflags.append("-DNEW_COMPACTION_BEHAVIOR=1")
  gcstats.target = "memwatch"
  gcstats.source = """
    src/memwatch.cc
    src/heapdiff.cc
    src/util.cc
    src/init.cc
  """

def test(t):
  Utils.exec_command('make test')
