var results = "";

function Test(file, mask)
{
	if (results != "")
	{
		results += "\n";
	}
	
	results += file;
	try
	{
		if (Salamander.MatchesMask(file, mask))
		{
			results += " matches ";
		}
		else
		{
			results += " does not match ";
		}
	}
	catch (e)
	{
		results += " *** exception *** ";
	}
	
	results += mask;
}

Test("autoexec.bat", "*.bat");
Test("C:\\autoexec.bat", "*.bat");
Test("document.txt", "||*.txt");
Test("document.txt", "*.txt");
Salamander.MsgBox(results);
