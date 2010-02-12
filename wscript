# -*- mode: python; -*- 


import os
import sys
sys.path.append('./wtools')
import Options
import Configure
import commands

APPNAME  = 'poloka'
VERSION  = '0.1.0'
srcdir   = '.'

def find_arch():
    # find the architecture
    import Configure
    if commands.getstatus('which fs'):
        ret = commands.getstatusoutput('fs sys')[1]
        blddir = 'build.'+ ret.split("'")[-2]
    else:
        ret = os.uname()
        blddir = 'build.'+ ret[0]+'-'+ret[-1]
    return blddir

blddir   = find_arch()



def set_options( ctx ):
    
    ctx.tool_options('compiler_cc')
    ctx.tool_options('compiler_cxx')
    ctx.tool_options('compiler_fortran', tooldir='./wtools')
    ctx.tool_options('flex')
    ctx.tool_options('bison')
    ctx.add_option('--with-cfitsio', action='store', default='/home/betoule/soft/cfitsio/v3r006/Linux-i686/', help='Path to the cfitsio root dir')
    ctx.add_option('--with-sex', action='store', default='/home/betoule/soft/sex/v222/Linux-i686/', help='Path to the cfitsio root dir')
    

def configure( conf ):
    
    # fortran compilers (not included by default in waf-1.5)
    conf.check_tool( 'compiler_fortran', tooldir='./wtools' )
    
    # c compiler 
    conf.check_tool( 'compiler_cc' )
    # TODO: why is -fPIC -DPIC not specified by default for gcc ? 
    #       is there a SharedObject() method like in scons ? 
    conf.env['CCFLAGS'] = [ '-fPIC', '-DPIC' ]
    
    # c++ compiler 
    conf.check_tool( 'compiler_cxx' )
    
    # flex and bison
    conf.check_tool( 'flex' )
    conf.env['FLEXFLAGS'] = '' 
    conf.check_tool( 'bison' )
    conf.env['BISONFLAGS'] = ['-y', '-l', '-d']
    
    # file substitutions
    conf.check_tool( 'misc' )
    
    # various headers & libraries 
    conf.check_cc( header_name='math.h' )
    conf.check_cc( lib='z', msg='checking for zlib' )    
    
    # lapack
    if conf.check_cc( lib='lapack', msg='checking for lapack' ):
        conf.env.global_lapack = True
    else:
        conf.env.global_lapack = False
    
    # cernlib 
    try:
        conf.find_program( 'cernlib', mandatory = True )
        conf.check_cfg( path='cernlib', args='', 
                        package='mathlib packlib',
                        uselib_store='cern' )
    except Configure.ConfigurationError:
        conf.fatal('CERNLIB not found.')
        
        
    # pkg config 
    conf.find_program('pkg-config')
    conf.env.CPPPATH_CFITSIO=[Options.options.with_cfitsio+'../src/']
    conf.env.CPPPATH_SEX=[Options.options.with_sex+'../src/',Options.options.with_sex+'../src/wcs',Options.options.with_sex+'../src/fits']
    conf.env.LIBPATH_CFITSIO=[Options.options.with_cfitsio]
    conf.env.LIBPATH_SEX=[Options.options.with_sex]
    conf.env.LIB_CFITSIO='cfitsio'
    conf.env.LIB_SEX=['sex','fits','wcs']

#     # sextractor 
#     try:
#         conf.check_cfg( path='sex-config',
#                         args = '--cflags --libs', 
#                         package = 'sextractor', 
#                         mandatory = True )
#     except Configure.ConfigurationError:
#         conf.fatal('unable to locate sextractor.')
        
#     # cfitsio 
#     try:
#         conf.check_cfg( path='cfitsio-config', 
#                         args = '--cflags --libs', 
#                         package = 'cfitsio', 
#                         mandatory = True )
#     except Configure.ConfigurationError:
#         conf.fatal('unable to locate cfitsio')
        
    


def build( bld ):
    
    bld.add_subdirs( [ "cern_stuff",
                       "src_base",
                       "src_utils", 
                       "cern_utils",
                       "src", 
                       "psf", 
                       "flat", 
                       "lc",
                       "utils" ] )
    if not bld.env.global_lapack:
        bld.add_subdirs(["lapack_stuff"])
    # if the thing is not installed 
    
#     obj = bld( 'subst', 
#                target = 'poloka.pc', 
#                source = 'poloka.pc.in' )
    
#     obj.dict = { 'NAME': APPNAME, 
#                  'VERSION': VERSION, 
#                  'REQUIREMENTS': '',
#                  'CONFLICTS': '',
#                  'LIBS': bld.env.LIBS, 
#                  'LIBS_PRIVATE': '', 
#                  'CFLAGS': bld.env.CFLAGS }

#     dd = obj.env.get_merged_dict()
#     import pprint
#     pprint.pprint( dd )
#     print bld.path.__class__
