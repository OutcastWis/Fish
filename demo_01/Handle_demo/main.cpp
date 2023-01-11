#include "pch.h"

#include "stl_import.h"

#include "op.h"


int main()
{
    auto i = op::PrivateNamespace();
    if (!i.IsEmpty())
        _tprintf(_T("%s"), static_cast<const TCHAR*>(i));
    else
    {
        while (true) {
            TCHAR cmd[256] = {};
            _tscanf_s(_T("%s"), cmd);
            if (_tcsnicmp(cmd, _T("exit"), 5) == 0)
                break;
        }
    }

    op::ClearPrivateNamespace();

    return 0;
}
