#!/usr/bin/env python

import cgi
import os, tempfile, re, sys
import subprocess
import sys, ConfigParser
import json

form = cgi.FieldStorage()

# The code itself
code = ''
if form.has_key('code'):
  code = form['code'].value

# Which program do we use?
program = ''
if form.has_key('program'):
  program = form['program'].value

def get_arg(name):
  if form.has_key(name):
    return form[name].value
  return ''

# Print the content type now: if we need to do any errors, we can just hit
# print and not worry about stuff
print 'Content-Type: text/plain\n'

tool_dict = {
  'dehydra': ['%(gccdir)s/gcc', '-c', '-o', '/dev/null', '-fplugin=%(dehydra)s',
    '%(fshortwchar)s',
	'-fplugin-arg-gcc_dehydra-script=scripts/dehydra-dump.js'],
  'dehydra-cpp': ['-x', 'c++', '%(filename)s'],
  'dehydra-c': ['-x', 'c', '%(filename)s'],
  'jshydra': ['%(jshydra)s'],
  'jshydra-parse': ['scripts/jshydra-parse.js', '%(filename)s'],
  'jshydra-ast': ['scripts/jshydra-ast.js', '--trueast', '%(filename)s'],
}

param_map = {
  'fshortwchar': ('','-fshort-wchar'),
  'std': ('', '-std=%(val)s')
}

if not (program in tool_dict):
  print "Unknown program %s" % (program)
  sys.exit(1)

config = ConfigParser.ConfigParser()
config.read('./viewsource.config')

confdict = dict(config.items('Tools'))
for key in param_map:
  if form.has_key(key):
    confdict[key] = param_map[key][1] % {'val': form[key].value}
  else:
    confdict[key] = param_map[key][0]

entry = tool_dict[program]
if '-' in program:
  prog_no_opts = program.split('-')[0]
  if prog_no_opts in tool_dict:
    base = tool_dict[prog_no_opts]
    base.extend(entry)
    entry = base
text = ''

try:
  tmp = tempfile.NamedTemporaryFile(mode='w+t', suffix='')
  tmp.writelines([code,'\n'])
  tmp.seek(0)
  confdict['filename'] = tmp.name

  args = [e for e in (e % confdict for e in entry) if e != '']

  stdout = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
  text = stdout.read()
finally:
  tmp.close()

if text == '':
  text = 'There was a problem analyzing your code. No Output.'

print text

