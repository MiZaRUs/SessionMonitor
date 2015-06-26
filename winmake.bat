rem 
path=%PATH%;"C:\cygwin64\bin"
cd symon
c:\cygwin64\bin\make.exe
rem move *.exe E:\SM\test\
del *.o
cd ..
cd SMonitor\icons
c:\cygwin64\bin\windres.exe -i icons.rc -o icons.o
move icons.o ..\
cd ..
c:\cygwin64\bin\make.exe
rem move *.exe E:\SM\test\
del *.o

