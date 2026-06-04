
                    @echo off 
                     
                    if not exist build mkdir build 
                    if not exist data mkdir data 
                    SET COMMON_COMPILER_OPTIONS=-FC -Od -Zi -nologo  
                    SET COMMON_LINKER_OPTIONS=-incremental:no -ignore:4099 
                    SET WIN_LINKER_OPTIONS=user32.lib   	 
                    SET MAIN_NAME=win_exrviewer.cpp 
  
del build\*.pdb 
pushd build 
 
cl %COMMON_COMPILER_OPTIONS%  ..\src\%MAIN_NAME% /link %WIN_LINKER_OPTIONS% %COMMON_LINKER_OPTIONS% 
 
popd 

