������ �������:
VS2013, ������������ Release Win32.

��������� ��������� ������:
1. ������������� mysql (���� ������ ����� ���� �� ������ ��������, ������� ����� ��������� �� ������ �������� �������).
  - ������ ����� �������� � ��������� �������� ���� �� ����� �����, �� ���� ��������, ��� ��� ���������� ����� ����� ��������� �������� ���������������� ������ ������ ��������.
2. ������� ���� �������� db/schema.sql (����� ������� ������������ � ������ ��� ������ ������� - ��� ���� ��������� ��������� ������������ � ��������� �������� ������ � ������ ����).
3. �������� ����� �������: backserver.exe � backserver.ini 
  - � ��� ����� ������� ����, ������� ����� ������� ������ - ����� ������������ ������������ 80 ��� 443, ������� ������� � ����������� ��������
  - � ��� ���� ������� ��������� ��� ����������� � ��������� ����
4. ����������� ������ ������� � ���� ������� Windows (backserver -install ��� ���������� sc create) - ������ ����� ����������� ������������� � �������� ������������ �������.
5. ����������� ������� ��� ������� � ������� �������.
6. ��� ������, ��������� ������ (net start backserver ��� �� ���������� �����).
