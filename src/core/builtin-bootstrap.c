"\b\b\b\003\263builtin;\002\2040.3.7;\002\233\201\"builtin_c\";\003\201\"system\";\001\201\"io\";\b\231_eel_version=[__version(0),__version(1),__version(2)];\001\206__version(3)\001_eel_version[3]=__version(3);\006\231_lib_version=[0,3,7];\b\b\233\262eel_version\001{\001\205_eel_version;\001}\002\233\262builtin_version\001{\001\205_lib_version;\001}\b\233\262compile(s)[flags=0,filename=\241,modname=\241]\001{\001\230m=\263[];\001m.__source=s;\001\206filename\001m.__filename=filename;\001\207\001m.__filename=\"script from \"+((\261)\245s)+\", \"+\001(\261)\246s+\" bytes\";\001\206modname\001m.__modname=modname;\001\207\001m.__modname=m.__filename;\001\206\250(flags & SF_NOCOMPILE)\001{\001__compile(m,flags);\001\206\250(flags & SF_NOINIT)\001m.__init_module();\001}\001\205m;\001}\004\233\262__load_eel_module(fullpath,flags)\001{\001\230f=file[fullpath,\"rb\"];\001\230buf=read(f,\246f);\001\205compile(buf,flags,fullpath);\001}\002$.__load_eel_module=__load_eel_module;\001$.__load_binary_module=__load_binary_module;\004\233\262__load_via_path_modules(modname,flags)\001{\003\262try_load(p,f)\001{\001\230ldeel=$.__load_eel_module;\001\230ldbin=$.__load_binary_module;\001\222\001\206copy(p,\246p-4)!=\".eel\"\001\205ldeel(p+\".eel\",f);\001\222\001\206copy(p,\246p-4)!=\".ess\"\001\205ldeel(p+\".ess\",f);\001\222\001\205ldeel(p,f);\001\222\001\206copy(p,\246p-\246SOEXT)!=SOEXT\001\205ldbin(p+SOEXT,f);\001\222\001\205ldbin(p,f);\001\205\241;\001}\001\230modpaths=$.path_modules;\001\213\230i=0,\246modpaths-1\001{\001\206\246modpaths[i]\001\230m=try_load(modpaths[i]+DIRSEP+modname,\001flags);\001\207\001m=try_load(modname,flags);\001\206m\001\205m;\001}\001\225__exports().XMODULELOAD;\001}\003\233\262__load_injected_module(modname,flags)\001{\001\205$.injected_modules[modname](flags);\001}\004$.module_loaders=[\001__get_loaded_module,\001__load_injected_module,\001__load_via_path_modules\001];\004\233\262load(modname)[flags=0]\001{\001\230x=$.module_loaders;\001\213\230i=0,\246x-1\001{\001\230load_error=\241;\001\222\001{\001\230m=x[i](modname,flags);\001\206\245m==\263\001\205m;\001load_error=\"load(): \"+x[i].name+\"(\\\"\"+\001(\261)modname+\"\\\"\"+\", \"+\001(\261)flags+\") returned \"+\001(\261)m+\" instead of a module\";\001}\001\206load_error\001\225load_error;\001}\001\225\"load(): Could not load module \\\"\"+(\261)modname+\"\\\"\";\001}\b\001\233\262exception_info(x)\001{\001\230r=\265[];\001r.code=x;\001\222\001r.name=exception_name(x);\001\224\001r.name=\"\";\001\222\001r.description=exception_description(x);\001\224\001r.description=\"(No description available.)\";\001\205r;\001}\002\222\001\213\230x=0,99999\001__exports()[exception_name((\256)x)]=(\256)x;\006\233\262deepclone(src)[level=0]\001{\001\206level>1000\001\225\"deepclone(): Infinite recursion aborted!\";\001\210\245src\001\211\264\001{\001\230a=[];\001\213\230i=0,\246src-1\001a[i]=deepclone(src[i],level+1);\001\205a;\001}\001\211\265\001{\001\230t=\265[];\001\213\230i=0,\246src-1\001t[key(src,i)]=deepclone(index(src,i),level+1);\001\205t;\001}\001\212\001\205\247src;\001}\006\233\262deepcompare(l,r)[level=0]\001{\001\206level>1000\001\225\"deepcompare(): Infinite recursion aborted!\";\001\210\245l\001\211\264\001{\001\206\245r!=\264\001\205\240;\001\206\246l!=\246r\001\205\240;\001\213\230i=0,\246l-1\001\206\250deepcompare(l[i],r[i],level+1)\001\205\240;\001\205\237;\001}\001\211\265\001{\001\206\245r!=\265\001\205\240;\001\206\246l!=\246r\001\205\240;\001\213\230i=0,\246l-1\001{\001\230k=key(l,i);\001\206\250deepcompare(l[k],r[k],level+1)\001\205\240;\001}\001\205\237;\001}\001\212\001\205l==r;\001}\b\003$.cleanup=[];\002\233\236__cleanup\001{\001\213\230i=0,\246$.cleanup-1\001$.cleanup[i]();\001delete($);\001}\n"