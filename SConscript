#
# SConscript
#
# Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted
# provided that the following conditions are met:
#
#     1.  Redistributions of source code must retain the above copyright notice, this list of
#         conditions and the following disclaimer.
#     2.  Redistributions in binary form must reproduce the above copyright notice, this list
#         of conditions and the following disclaimer in the documentation and/or other materials
#         provided with the distribution.
#     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
#         or promote products derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

print "================================"
print "= Reading top-level SConscript ="
print "================================"
print 

Import(['clean_envs'])

tools_env = clean_envs['userspace'].Clone()

tools_env['LIBS'].append('accessories/libaccessories.a')
tools_env['LIBS'].append('lights/liblights.a')
tools_env['LIBS'].append('lights/rpi_ws281x/libws2811.a')
tools_env['LIBS'].append('audio/libaudio.a')
tools_env['LIBS'].append('audio/lib/libfft.a')
tools_env['LIBS'].append('log/log.o')

srcs = Split('''
    main.cpp
''')

if ARGUMENTS.get('VERBOSE') != '1':
    tools_env['CCCOMSTR'] = "Compiling $TARGET"
    tools_env['LINKCOMSTR'] = "Linking $TARGET"

objs = []
for src in srcs:
   objs.append(tools_env.Object(src))


# Additional Compiler Flags
#additionalFlags = tools_env.ParseFlags("-Ilights -Iaudio -Ilights/rpi_ws281x -Iaudio/lib -Iaccessories -Ilog -g -O0")
#tools_env.MergeFlags(additionalFlags)

test = tools_env.Program('test', objs + tools_env['LIBS'], LIBS=tools_env['LIBS'], CCFLAGS=tools_env['CCFLAGS'])

tools_env.Default([test])
