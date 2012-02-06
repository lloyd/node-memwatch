# -*- mode: python -*-

import Options, Utils, sys

srcdir = "."
blddir = "build"
VERSION = "0.0.1"

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
  gcstats.target = "gcstats"
  gcstats.source = """
    src/gcstats.cc
  """

def test(t):
  Utils.exec_command('make test')
