сборка - VS2013.

backvnc - библиотеки vnc (об€зательно собирать х86 и х64). адрес сервера и идентификатор бота на сервере прописываютс€ в файле vncdll/client.c (тестовый алгоритм, правильнее передавать их из бота, как параметры).

testvnc - стартовый exe/dll дл€ их запуска. правильнее запускать backvnc пр€мо из бота (пользоватьс€ исходным кодом только в качестве примера).

ѕор€док сборки:
1. backvnc, конфигурации х86 и х64.
2. копируем backvnc/Release/Win32/vncdll32.dll и backvnc/Release/x64/vncdll64.dll в testvnc/bin
3. testvnc, конфигураци€ Win32 (будет включать в себ€ обе библиотеки vnc и работать на Windows любой разр€дности).


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