# coding:utf-8

'''
Created on 2017-04-11

@author: vincent
'''

import os
import sys
import shutil
import re
# import subprocess

############ Config Zone ##################

# 相关最小配置
class cls_minimum_config:
    def __init__(self):
        self.tool_cmd = r'''./tiny-lib-tool''' # 在系统路径中,则去掉./

############ Global env ##################
class cls_default_config(cls_minimum_config):
    def __init__(self):
        cls_minimum_config.__init__(self)
        self.work_dir = r''''''
        self.src_dir = r'''input'''
        self.dst_dir = r'''output'''
        self.dst_src_dir = r'''output/src'''
        self.proj_dir = r'''libSim.xcodeproj'''
        self.tmp_dir = r'''tmp'''
        self.tmp_input_fn = r'''inputCpp.cpp'''
        self.tmp_output_fn = r'''outputCpp.cpp'''

        self.sysroot = ""
        self.resdir = ""
        # self.sysroot = r'''-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk'''
        # self.resdir = r'''-resource-dir /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../lib/clang/8.0.0'''

        self.ruby_cmd = r'''./proj_tool.rb'''

# 相关的正则表达式
class cls_re_pattern:
    def __init__(self):
        self.class_p = re.compile(r'''(?i)(@interface\s+(\w+)\s*?(?:.*?)$.*?^@end)''', re.S|re.M)
        self.return_p = re.compile(r'''(?i)(?:return\s*?@\"(.*?)\";)''', re.S)
        self.class_imp_p = re.compile(r'''(?i)(@implementation\s*?(\w*?)\s*?\n.*?@end)''', re.S)
        self.property_p = re.compile(r'''(?i)(?:@property.*?\)\s*?(\w.*?);)''', re.S)
        self.func_p = re.compile(r'''(?i)(^\s*[-|+]\s*?\((.*?)\)(\w*?)(.*?)\s*?);''', re.S|re.M)
        self.func_parm_p = re.compile(r'''\((.*?)\)(\w*)''', re.S)

        self.sysroot_p = re.compile(r'''(?i)(?:-isysroot"\s"(.+?)")''', re.S|re.M)
        self.res_dir_p = re.compile(r'''(?i)(?:-resource-dir"\s"(.+?)")''', re.S|re.M)

gConf = cls_default_config()
gPattern = cls_re_pattern()


# 生产objc的空实现函数
def implementAllObjcFun(classStr):
    funs = re.findall(gPattern.func_p, classStr)

    outFuns = []
    for f in funs:
        retStr = f[0]
        newFun = retStr
        if f[1]=='void':
            newFun += " {\n"+4*" "+"return;\n}\n\n"
        else:
            newFun += " {\n"+4*" "+"return 0;\n}\n\n"
        outFuns.append(newFun)
    return outFuns

# 处理objc头文件,使用正则处理
def dealObjcHead(ddir, sf):
    baseName=os.path.basename(sf)

    # 拷贝头文件
    shutil.copy(sf, ddir)
    print "Objc header: %s" % baseName

    # 生产实现文件
    df = ddir + os.sep + re.sub('.h$', '.m', baseName)

    content = open(sf, 'r').read()
    allClass = re.findall(gPattern.class_p, content)
    dfp = open(df, 'w')

    dfp.write('#import "'+baseName+'"\n\n')
    for c in allClass:
        dfp.write("@implementation " + c[1] + "\n\n");

        funs = implementAllObjcFun(c[0])
        for f in funs:
            dfp.write(f);
        dfp.write("\n@end\n\n")
    dfp.close()

# 处理cpp头文件,使用
def dealCppHeader(ddir, sf):
    baseName=os.path.basename(sf)

    impf = ddir+os.sep+re.sub('.h$','.cpp',baseName)
    if os.path.isfile(impf):
        os.remove(impf)

    # 拷贝头文件
    shutil.copy(sf, ddir)
    print "Cpp header: %s" % baseName

    # 拷贝头文件并重命名
    inf = gConf.tmp_dir+os.sep+gConf.tmp_input_fn
    if os.path.isfile(inf):
        os.remove(inf)
    shutil.copyfile(sf, inf)

    outf = gConf.tmp_dir+os.sep+gConf.tmp_output_fn
    if os.path.isfile(outf):
        os.remove(outf)

    # 调用shell命令

    cmd = "%s %s -- %s %s" % (gConf.tool_cmd, gConf.tmp_input_fn, gConf.sysroot, gConf.res_dir)
    print "cmd: %s" % cmd
    os.chdir(gConf.tmp_dir)
    os.system(cmd)
    os.chdir(gConf.work_dir)

    if os.path.isfile(outf):
        impfd = open(impf, 'w')
        content = open(outf, 'r').read()
        content = '#include "'+baseName+'"\n\n' + content
        impfd.write(content)
        impfd.close()
        # shutil.copyfile(outf, impf)
    else:
        print "Parse cpp header failed!!!"


def dealFile(sf):
    sdir = os.path.abspath(os.path.dirname(sf))
    s = sdir[(len(gConf.src_dir)-len(sdir)) or len(sdir):]
    ddir = gConf.dst_src_dir + os.sep + s

    content = open(sf, 'r').read()
    allClass = re.findall(gPattern.class_p, content)

    if allClass != None and len(allClass):
        dealObjcHead(ddir, sf)
    else:
        dealCppHeader(ddir, sf)

    return

def dealDir(sd):
    s1 = os.path.abspath(sd)
    s = s1[(len(gConf.src_dir)-len(s1)) or len(s1):]
    dd = gConf.dst_src_dir + os.sep + s
    os.makedirs(dd)

    return

def buildProj(build_dir):
    proj = build_dir+os.sep+os.path.basename(gConf.proj_dir)
    if os.path.isdir(proj):
        shutil.rmtree(proj)
    shutil.copytree(gConf.proj_dir,proj)

    # 调用ruby脚本
    # os.system("/usr/bin/ruby %s" % (rubyScript))

    pass

def initDir():
    global gConf
    currentDir = os.getcwd()
    # currentDir = r'''/work/github/mymy/tinyLibTool'''
    gConf.work_dir = currentDir
    gConf.src_dir = currentDir+os.sep+gConf.src_dir
    gConf.dst_dir = currentDir+os.sep+gConf.dst_dir
    gConf.dst_src_dir = currentDir+os.sep+gConf.dst_src_dir
    gConf.proj_dir = currentDir+os.sep+gConf.proj_dir
    gConf.tmp_dir = currentDir+os.sep+gConf.tmp_dir

    # 清空输出目录
    if os.path.isdir(gConf.dst_dir):
        shutil.rmtree(gConf.dst_dir)
    os.makedirs(gConf.dst_dir)

    # 清空tmp目录
    if os.path.isdir(gConf.tmp_dir):
        shutil.rmtree(gConf.tmp_dir)
    os.makedirs(gConf.tmp_dir)

    if gConf.tool_cmd.startswith('./'):
        gConf.tool_cmd = currentDir+os.sep+gConf.tool_cmd[2:]

    print "input: %s,output: %s" % (gConf.src_dir, gConf.dst_dir)


# 判断clang、获取clang参数等
def configEnv():
    global  gConf
    initDir()

    if len(gConf.sysroot) and len(gConf.resdir):
        return

    os.chdir(gConf.tmp_dir)
    os.system("touch tmp.cpp")
    os.system("clang -### tmp.cpp 2>tmp.txt")
    rst = open('tmp.txt').read()

    res1 = re.search(gPattern.sysroot_p, rst)
    res2 = re.search(gPattern.res_dir_p, rst)

    if res1==None or res2==None:
        raise "No clang environment..."
        exit(1)
    gConf.sysroot = "-isysroot " + res1.groups()[0]
    gConf.res_dir = "-resource-dir " + res2.groups()[0]


if __name__ == '__main__':
    configEnv()

    if os.path.isdir(gConf.dst_src_dir):
        shutil.rmtree(gConf.dst_src_dir)
    os.makedirs(gConf.dst_src_dir)
    reload(sys)
    sys.setdefaultencoding('utf8')

    for root, dirs, files in os.walk(gConf.src_dir):
        for nd in [root + os.sep + d for d in dirs]:
            dealDir(nd)
        for nf in [root + os.sep + f for f in files if f.find('.h') > 0]:
            dealFile(nf)

    buildProj(gConf.dst_dir)
















