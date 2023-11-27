#pragma once

bool ProcessSeparatedList(LPTSTR & str)
{
	if (NULL == str)
		return true;

	int len = lstrlenW(str);
	str = (LPTSTR)realloc(str, (len + 2) * sizeof(TCHAR));
	if (str)
	{
		str[len + 1] = 0;
		for (int i = 0; i < len; i++)
		{
			if (str[i] == ';')
				str[i] = 0;
		}
	}

	return (NULL != str);
}
