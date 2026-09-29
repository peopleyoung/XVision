"""POSIX flow fixtures with mock Windows tools; not Windows runtime acceptance."""
import argparse,os,pathlib,shutil,subprocess,tempfile,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pwsh',default=shutil.which('pwsh'),help='PowerShell 7 executable')
PWSH=parser.parse_args().pwsh
if os.name!='posix' or not PWSH:
    parser.error('Run on POSIX with PowerShell 7 installed or pass --pwsh.')
def put(path,text='fixture',exe=False):
    path.parent.mkdir(parents=True,exist_ok=True); path.write_text(text)
    if exe: path.chmod(0o755)
def case(name,config='Release',skip=False,skip_build=True,fault=''):
    with tempfile.TemporaryDirectory(prefix='xvision-package-flow-') as tmp:
        r=pathlib.Path(tmp); src=r/'XVision'; scripts=src/'scripts'; scripts.mkdir(parents=True)
        for f in ['Package-WindowsBuild.ps1','Verify-WindowsBuild.ps1','WindowsBuild.Common.ps1']:
            shutil.copy2(ROOT/'XVision/scripts'/f,scripts/f)
        shutil.copytree(ROOT/'XVision/XVision/Res/translations',src/'XVision/Res/translations')
        catalog=r/'doc/开发工作/算子目录.csv'
        catalog.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/'doc/开发工作/算子目录.csv',catalog)
        build=src/'build'/('windows-msvc2019-'+config.lower()); bins=build/('BinD' if config=='Debug' else 'Bin')
        suffix='d' if config=='Debug' else ''
        put(bins/'XVision.exe'); put(bins/'XvFuncCollection/XvFuncSystem.dll')
        for x in ['XvCamera','XvCore','XvData','XvDisplay','XvTokenMsg','XvUtils','XWidget','XLog','XLanguage','AdsDocking','XFlowGraphics','XConcurrent','halcon','halconcpp','onnxruntime','opencv_world4100']: put(bins/(x+'.dll'))
        sdk=r/'SDK with spaces'; put(sdk/'opencv_world4100.dll'); put(sdk/'onnxruntime.dll')
        flags=['BUILD_TESTING','XVISION_BUILD_COMMON_USING','XVISION_BUILD_SYSTEM_PLUGIN','XVISION_ENABLE_BREAKPAD','XVISION_ENABLE_OPENCV','XVISION_ENABLE_ONNXRUNTIME']
        cache=[f'{f}:BOOL=ON' for f in flags]+[f'XVISION_OPENCV_RUNTIME_DLL:FILEPATH={sdk}/opencv_world4100.dll',f'XVISION_ONNXRUNTIME_RUNTIME_DLL:FILEPATH={sdk}/onnxruntime.dll']
        if fault=='disabled': cache=[s.replace('XVISION_ENABLE_ONNXRUNTIME:BOOL=ON','XVISION_ENABLE_ONNXRUNTIME:BOOL=OFF') for s in cache]
        put(build/'CMakeCache.txt','\n'.join(cache))
        if fault=='missing_backend': (bins/'onnxruntime.dll').unlink()
        vc=r/'Visual Studio with spaces'/'VC'
        crt=vc/'Redist/MSVC/14.44.35208'/('Debug_NonRedist/x64/Microsoft.VC143.DebugCRT' if config=='Debug' else 'x64/Microsoft.VC143.CRT')
        crt_names=[f'msvcp140{suffix}.dll',f'vcruntime140{suffix}.dll',f'vcruntime140_1{suffix}.dll',f'concrt140{suffix}.dll']
        for filename in crt_names: put(crt/filename)
        if fault=='missing_msvc': (crt/f'vcruntime140_1{suffix}.dll').unlink()
        qt=r/'Qt'; put(qt/'lib/cmake/Qt6/Qt6Config.cmake'); put(qt/f'bin/Qt6Test{suffix}.dll')
        qt_modules=['Core','Gui','Widgets','Xml','Concurrent','Qml','Network','SerialPort','SerialBus','Sql','StateMachine']
        missing_qt={'missing_qml':'Qml','missing_sql':'Sql','missing_concurrent':'Concurrent','missing_state_machine':'StateMachine'}.get(fault)
        for module in qt_modules:
            if module!=missing_qt: put(qt/f'bin/Qt6{module}{suffix}.dll','qt-sdk-module')
        put(qt/'bin/qmake.exe','#!/bin/sh\nprintf "6.4.0\\n"\n',True)
        put(qt/'bin/windeployqt.exe','''#!/usr/bin/python3
import os,pathlib,sys
args=sys.argv[1:]; assert 'sqlite' not in args
assert args[-1].endswith('XvFuncCollection/XvFuncSystem.dll')
out=pathlib.Path(args[args.index('--dir')+1]); suffix='d' if '--debug' in args else ''
for module in ['Concurrent','StateMachine']:
    filename=out/('Qt6'+module+suffix+'.dll')
    assert str(filename) in args and filename.is_file()
# Match native dependency scanning: unused Concurrent/StateMachine are omitted.
for m in ['Core','Gui','Widgets','Xml','Network','SerialPort','SerialBus','Sql']:
    (out/('Qt6'+m+suffix+'.dll')).write_text('fixture')
for part,base in [('platforms','qwindows'),('sqldrivers','qsqlite')]:
    (out/part).mkdir(exist_ok=True)
    s='' if base=='qwindows' and os.environ['FAULT']=='wrong_debug' else suffix
    (out/part/(base+s+'.dll')).write_text('fixture')
''',True)
        put(build/f'tests/{config}/XvSystemPluginTests.exe','''#!/usr/bin/python3
import os,pathlib,sys
assert len(sys.argv)==3
catalog=pathlib.Path(sys.argv[2])
assert catalog.name=='算子目录.csv'
lines=catalog.read_text(encoding='utf-8-sig').splitlines()
assert lines[0]=='entry_id,canonical_role,property,value,display_name,category' and len(lines)==124
assert '/Qt/' not in os.environ['PATH'] and '/build/' not in os.environ['PATH']
pathlib.Path(os.environ['MARKER']+'.plugin').write_text('called')
sys.exit(17 if os.environ['FAULT']=='plugin_fail' else 0)
''',True)
        tools=r/'tools'
        put(tools/'cmake','''#!/usr/bin/python3
import os,pathlib,sys
if '--version' in sys.argv:
    print('cmake version 3.25.1')
elif '--build' not in sys.argv:
    assert '-DXVISION_ENABLE_OPENCV=ON' in sys.argv
    assert '-DXVISION_ENABLE_ONNXRUNTIME=ON' in sys.argv
    pathlib.Path(os.environ['MARKER']+'.configure').write_text('called')
''',True)
        put(tools/'powershell.exe','''#!/usr/bin/python3
import os,pathlib,sys
assert '-FullFeatures' in sys.argv
if '-SkipBuild' not in sys.argv:
    assert sys.argv[sys.argv.index('-OpenCvRoot')+1].endswith('SDK with spaces')
    assert sys.argv[sys.argv.index('-OnnxRuntimeRoot')+1].endswith('SDK with spaces')
pathlib.Path(os.environ['MARKER']+'.verify').write_text('called')
sys.exit(9 if os.environ['FAULT']=='verify_fail' else 0)
''',True)
        put(tools/'git','#!/bin/sh\nprintf "fixture-commit\\n"\n',True)
        env=dict(os.environ,OS='Windows_NT',QTDIR=str(qt),VCINSTALLDIR=str(vc),SystemRoot=str(r/'Windows'),PATH=str(tools)+':'+str(tools)+':/usr/bin',FAULT=fault,MARKER=str(r/'called'))
        cmd=[PWSH,'-NoProfile','-File',str(scripts/'Package-WindowsBuild.ps1'),'-Configuration',config,'-OpenCvRoot',str(sdk),'-OnnxRuntimeRoot',str(sdk),'-KeepStaging']
        if skip_build: cmd+=['-SkipBuild']
        if skip: cmd+=['-SkipTests']
        p=subprocess.run(cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=45)
        archive=src/'dist'/f'XVision-{config}-win64.zip'
        if fault:
            assert p.returncode!=0,(name,p.stdout)
            assert not archive.exists(),(name,'published despite fault')
            expected={'missing_msvc':'Missing MSVC runtime','disabled':'XVISION_ENABLE_ONNXRUNTIME=ON','missing_backend':'enabled backend runtime','missing_qml':'Qt6Qml.dll','missing_sql':'Qt6Sql.dll','missing_concurrent':'Qt6Concurrent.dll','missing_state_machine':'Qt6StateMachine.dll','wrong_debug':'qwindowsd.dll','plugin_fail':'Packaged plugin failed','verify_fail':'First-party tests for the existing Windows build failed'}[fault]
            assert expected in p.stdout,(name,p.stdout)
        else:
            assert p.returncode==0,(name,p.stdout)
            assert archive.exists(),(name,p.stdout)
            manifest=(src/'dist'/f'XVision-{config}-win64'/'PACKAGE-MANIFEST.txt').read_text(encoding='utf-8-sig')
            assert ('SKIPPED - package not runtime-verified' in manifest)==skip
            assert (r/'called.plugin').exists()==(not skip)
            assert (r/'called.verify').exists()==(not skip)
            if skip and not skip_build: assert (r/'called.configure').exists()
            with zipfile.ZipFile(archive) as z:
                names=z.namelist(); assert 'XVision.exe' in names
                assert not any(pathlib.PurePosixPath(n).name.lower().startswith('halcon') for n in names)
                assert 'licenses/qt-translations/qtbase_zh_CN.ts' in names
                assert 'licenses/qt-translations/GPL-3.0-only.txt' in names
                assert 'XvFuncCollection/XvFuncSystem.dll' in names
                assert all(filename in names for filename in crt_names)
                assert all(f'Qt6{module}{suffix}.dll' in names for module in qt_modules)
                for module in ['Concurrent','StateMachine']:
                    assert z.read(f'Qt6{module}{suffix}.dll')==b'qt-sdk-module'
                assert not any('XvSystemPluginTests' in n or 'Qt6Test' in n for n in names)
        print('PASS',name,flush=True)
case('Release with verified staging')
case('Debug with correct DLL suffixes',config='Debug')
case('Explicitly skipped tests are recorded',skip=True)
case('Fresh build forwards SDK roots',skip_build=False)
case('Skipped tests still enable backends',skip=True,skip_build=False)
for fault in ['missing_msvc','disabled','missing_backend','missing_qml','missing_sql','missing_concurrent','missing_state_machine','wrong_debug','plugin_fail','verify_fail']:
    case('Reject '+fault,config='Debug' if fault=='wrong_debug' else 'Release',fault=fault)
print('14 packaging flow fixtures passed; mock tools do not establish Windows runtime acceptance.')
