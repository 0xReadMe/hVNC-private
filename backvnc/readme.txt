������ - VS2013.

backvnc - ���������� vnc (����������� �������� �86 � �64). ����� ������� � ������������� ���� �� ������� ������������� � ����� vncdll/client.c (�������� ��������, ���������� ���������� �� �� ����, ��� ���������).

testvnc - ��������� exe/dll ��� �� �������. ���������� ��������� backvnc ����� �� ���� (������������ �������� ����� ������ � �������� �������).

������� ������:
1. backvnc, ������������ �86 � �64.
2. �������� backvnc/Release/Win32/vncdll32.dll � backvnc/Release/x64/vncdll64.dll � testvnc/bin
3. testvnc, ������������ Win32 (����� �������� � ���� ��� ���������� vnc � �������� �� Windows ����� �����������).


1. Open backvnc/vncdll/client.c
   See on 250 line (here need write you ip addr)
2. Open backvnc/vnc.sln (Project file)
3. Build for x86 and x64
4. Copy files (backvnc/Release/x64/vncdll64.dll) and (backvnc/Release/Win32/vncdll32.dll)
   To (testvnc/bin), after select two files: vncdll32.h, vncdll32.dll
   Drag two this files to bin2txt.exe

You may see the console, do not close it, it will close itself 
as the data from the .dll will be written to .h files.


5. Repeat procedure with files vncdll64.dll and vncdll64.h
6. Copy files vncdll32.h and vncdll64.h to (\backvnc\testvnc\testvnc)
7. Open Project file (\backvnc\testvnc\testvnc)
8. Build Project for Win32_Exe!